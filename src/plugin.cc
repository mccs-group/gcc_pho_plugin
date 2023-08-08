//! GCC phase reordering experiment plugin source

#include "stdio.h"
#include "string.h"

#include "gcc-plugin.h"
#include "plugin-version.h"

#include "tree-pass.h"
#include "context.h"
#include "pass_manager.h"
#include "function.h"
#include <typeinfo>

/// Symbol required by GCC
int plugin_is_GPL_compatible;

extern char *concat(const char*, ...);
extern opt_pass *current_pass;
extern gcc::context* g;

/// All pass make fuctions specified as "extern", taken from tree-pass.h in gcc sources
#include "extern_makers.cc"
/// Auto-generated from pass_makers.conf include that matches pass name and its make function
#include "pass_makers.cc"

static struct plugin_info plugin_name = {"0.1", "GCC pass reorder experiment"};

/// Dummy pass class used for markers in pass tree
class dummy_pass: public opt_pass {
	public:
		dummy_pass(const pass_data& data, gcc::context* g) : opt_pass(data, g) {}

		opt_pass* clone() override {return new dummy_pass(*this);} 
};

/// Go through the pass tree and print name of every pass (unused)
static void print_pass_tree (opt_pass *pass) {
	while (pass) {
		
		print_pass_tree(pass->sub);

		printf("%s\n", pass->name);
		opt_pass *next = pass->next;
		pass = next;
	}
}

/// Gate callback, prints as much information about a pass as it can, human-readable
static void pass_dump_gate_callback(void* gcc_data, void* user_data) {
	if (current_pass) {
		printf("\n");
		printf("Name: %s\nProperties:\n\tRequired: 0x%x\n\tProvided: 0x%x\n\tDestroyed: 0x%x\n", 
				current_pass->name, 
				current_pass->properties_required, 
				current_pass->properties_provided, 
				current_pass->properties_destroyed);
		printf("Type: %d\n", current_pass->type);
		printf("Gate status: %d\n", *(int*)gcc_data);
		printf("Subpasses: %s", current_pass->sub ? "yes\n" : "no\n");
		printf("TODO before: 0x%X\n", current_pass->todo_flags_start);
		printf("TODO after: 0x%X\n", current_pass->todo_flags_finish);
		printf("Static number %d\n", current_pass->static_pass_number);
	}
}

/// Gate callback, prints pass data in format "name properties_required properties_provided properties_broken"
static void pass_dump_short_gate_callback(void* gcc_data, void* user_data) {
	if (current_pass) {
		printf("%s %d %d %d\n", current_pass->name, 
				current_pass->properties_required, 
				current_pass->properties_provided, 
				current_pass->properties_destroyed);
	}
}

/// Inserts marker pass in reference to any pass in pass tree, postfix arg append marker pass name to allow creation of unique passes
static void insert_marker_pass(struct plugin_name_args* plugin_info, enum opt_pass_type type, const char* name, enum pass_positioning_ops pos, int count = 1, const char* postfix = "") {
		struct pass_data dummy_pass_data = {
			type,
			concat("*plugin_dummy_pass", postfix, NULL),
			OPTGROUP_NONE,
			TV_PLUGIN_INIT,
			0,
			0,
			0,
			0,
			0,
		};
		opt_pass* dummy_pass_pass = new dummy_pass(dummy_pass_data, g);
		struct register_pass_info dummy_pass_info = {
			dummy_pass_pass,
			name,
			count,
			pos,
		};
	
		register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &dummy_pass_info);
}

/// Gate callback, overrides gate status of passes between markers, disabling them
static void clear_pass_tree_gate_callback(void* gcc_data, void* used_data) {
	static bool discard_flag = false;
	if (!strncmp("*plugin_dummy_pass", current_pass->name, 18)) {
		discard_flag = !discard_flag;
		return;
	}
	if (discard_flag) {
		*(bool*)gcc_data = 0;
	}
}

/// Pass execution callback, prints pass name. Can be used to determine which passes are actually being run
static void pass_exec_callback(void* gcc_data, void* used_data) {
	printf("%s\n", current_pass->name);
};

/// Parses pass list file and inserts passes from it into corresponding place in pass tree
static int register_passes_from_file(struct plugin_name_args* plugin_info, const char* filename) {
	FILE* passes_file = fopen(filename, "r");
	if (passes_file != NULL) {
		
		const char* ref_pass_name;
		if (strstr(filename, "list1.txt")) {
			ref_pass_name = "*plugin_dummy_pass_list1";
		} else if (strstr(filename, "list2.txt")) {
			ref_pass_name = "*plugin_dummy_pass_list2";
		} else if (strstr(filename, "list3.txt")) {
			ref_pass_name = "*plugin_dummy_pass_list3";
		} else {
			fprintf(stderr, "Unknown filename\n");
			return -1;
		}

		
		char* line = NULL;
		size_t len = 0;
		int res = getline(&line, &len, passes_file);
		while (res != -1) {
			if (line[res - 1] == '\n') {
				line[res - 1] = 0;
			}
			opt_pass* pass = pass_by_name(line);
			if (pass == NULL) {
				fprintf(stderr, "Failure reading pass file [%s], unknown pass name [%s]\n", filename, line);
				return -1;
			}
			if (pass->sub != NULL) {
				fprintf(stderr, "HUH?! [%s]\n", line);
				return -1;
			}
			struct register_pass_info pass_data = {pass, ref_pass_name, 1, PASS_POS_INSERT_BEFORE};

			
			char* subline = NULL;
			size_t sublen = 0;
			res = getline(&subline, &sublen, passes_file);
			opt_pass* prev_sub = NULL;
			while ((res != -1) && (subline[0] == '>')) {
				if (subline[res - 1] == '\n') {
					subline[res - 1] = 0;
				}
				opt_pass* subpass = pass_by_name(subline + 1);
				if (subpass == NULL) {
					fprintf(stderr, "Failure reading pass file, unknown pass name [%s]\n", subline + 1);
					return -1;
				}
				if (pass->sub == NULL) {
					pass->sub = subpass;
				} else {
					prev_sub->next = subpass;
				}
				prev_sub = subpass;
				
				res = getline(&subline, &sublen, passes_file);
			}
			register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_data);
			line = subline;
		}
		fclose(passes_file);
		free(line);
	} else {
		fprintf(stderr, "Plugin could not open passes file\n");
		return -1;
	}

	return 0;
}

/// Insert marker passes at the start and end of each pass list
static void insert_list_markers(struct plugin_name_args *plugin_info) {
    insert_marker_pass(plugin_info, opt_pass_type::GIMPLE_PASS, "inline_param", PASS_POS_INSERT_BEFORE, 1, "_list1");
    insert_marker_pass(plugin_info, opt_pass_type::GIMPLE_PASS, "inline_param", PASS_POS_INSERT_AFTER, 56);
    insert_marker_pass(plugin_info, opt_pass_type::GIMPLE_PASS, "*strip_predict_hints", PASS_POS_INSERT_AFTER, 1, "_list2");
    insert_marker_pass(plugin_info, opt_pass_type::GIMPLE_PASS, "local-pure-const", PASS_POS_INSERT_AFTER, 201);
    insert_marker_pass(plugin_info, opt_pass_type::RTL_PASS, "dfinit", PASS_POS_INSERT_AFTER, 1, "_list3");
    insert_marker_pass(plugin_info, opt_pass_type::RTL_PASS, "init-regs", PASS_POS_INSERT_BEFORE, 1);
}

/// Plugin initialization function, called by gcc to register plugin. Parses plugin arguments
int plugin_init(struct plugin_name_args *plugin_info, struct plugin_gcc_version *version) {
	register_callback(plugin_info->base_name, PLUGIN_INFO, NULL, &plugin_name);

	for (int i = 0; i < plugin_info->argc; i++) {
		//choose dump format based on "dump_format" (short/long/executed) argument
		if (!strcmp(plugin_info->argv[i].key, "dump_format")) {
			if (!strcmp(plugin_info->argv[i].value, "short")) {
				register_callback(plugin_info->base_name, PLUGIN_OVERRIDE_GATE, pass_dump_short_gate_callback, NULL);
			} else if (!strcmp(plugin_info->argv[i].value, "long")) {
				register_callback(plugin_info->base_name, PLUGIN_OVERRIDE_GATE, pass_dump_gate_callback, NULL);
			} else if (!strcmp(plugin_info->argv[i].value, "executed")) {
				register_callback(plugin_info->base_name, PLUGIN_PASS_EXECUTION, pass_exec_callback, NULL);
			} else {
				fprintf(stderr, "Incorrect plugin dump_format value\n");
				return -1;
			}
		}

		//choose pass remove operation
		if (!strcmp(plugin_info->argv[i].key, "pass_remove")) {
			if (!strcmp(plugin_info->argv[i].value, "clear")) {
				insert_marker_pass(plugin_info, GIMPLE_PASS, "*warn_unused_result", PASS_POS_INSERT_BEFORE, 1);
				register_callback(plugin_info->base_name, PLUGIN_OVERRIDE_GATE, clear_pass_tree_gate_callback, NULL);
			} else if (!strcmp(plugin_info->argv[i].value, "by_marker")) {
				for (int j = 0; j < plugin_info->argc; j++) {
					if (!strcmp(plugin_info->argv[j].key, "mark_pass_before")) {
						char* arg = xstrdup(plugin_info->argv[j].value);
						char* name = strtok(arg, ".");
						int type = atoi(strtok(arg, "."));
						insert_marker_pass(plugin_info, static_cast<opt_pass_type>(type), name, PASS_POS_INSERT_BEFORE, 1);
					}
					if (!strcmp(plugin_info->argv[j].key, "mark_pass_after")) {
						char* arg = xstrdup(plugin_info->argv[j].value);
						char* name = strtok(arg, ".");
						int type = atoi(strtok(arg, "."));
						insert_marker_pass(plugin_info, static_cast<opt_pass_type>(type), name, PASS_POS_INSERT_AFTER, 1);
					}
				}
				register_callback(plugin_info->base_name, PLUGIN_OVERRIDE_GATE, clear_pass_tree_gate_callback, NULL);
			} else {
				fprintf(stderr, "Incorrect plugin pass_reorder value\n");
				return -1;
			}
		}

		//choose pass replacement options
		if (!strcmp(plugin_info->argv[i].key, "pass_replace")) {
    			
			//insert required marker passes
            insert_list_markers(plugin_info);
			register_callback(plugin_info->base_name, PLUGIN_OVERRIDE_GATE, clear_pass_tree_gate_callback, NULL);

			for (int j = 0; j < plugin_info->argc; j++) {
				if (!strcmp(plugin_info->argv[j].key, "pass_file")) {
					if (register_passes_from_file(plugin_info, plugin_info->argv[j].value)) {
						return -1;
					}
				}
			}
		}

		//print help
		if (!strcmp(plugin_info->argv[i].key, "help")) {
			fprintf(stderr, "Plugin implementing gcc phase reorder\n"
							"To pass arguments to plugin use -fplugin-arg-plugin-<name>=<arg>\n"
							"Possible arguments:\n"
							"\tdump_format - choose pass dump format (to stdout):\n"
							"\t\tlong :     provides as much information about passes as possible. Prints passes even if they did not execute\n"
							"\t\tshort :    short, machine-readable format with information about pass properties. Prints passes even if they did not execute\n"
							"\t\texecuted : short, only pass names. Prints only executed passes\n\n"
							"\tpass_remove - remove passes from pass tree:\n"
							"\t\tclear : clear pass tree (compilation will fail with internal errors)\n"
							"\t\tby_marker : clear passes between markers, requires args mark_pass_before/mark_pass_after\n\n"
							"\tmark_pass_before/mark_pass_after - insert marker before/after pass, can only inster before/after the first instance of reference pass\n"
							"\t\treference pass name format is <name>.<type>, where type is from enum {GIMPLE, RTL, SIMPLE_IPA, IPA}\n\n"
							"\tpass_replace - remove optimization passes and read new passes to insert from lists\n\n"
		 					"\tpass_file - specifies files to take passes from. Possible file names are: \"list1.txt\", \"list2.txt\", \"list3.txt\".\n"
							"\t\tPlace where pass list is inserted is based on file name\n");
		}
	}
	return 0;
}

