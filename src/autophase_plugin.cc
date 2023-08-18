//! GCC phase reordering experiment plugin source

#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "autophase_plugin.hh"

/// Symbol required by GCC
int plugin_is_GPL_compatible;

extern char *concat(const char*, ...);
extern opt_pass *current_pass;
extern gcc::context* g;

// rtl_info_dumper rtl_dump(g);

static struct plugin_info plugin_name = {"0.1", "GCC Autophase characterisation"};

/// Pass execution callback, prints pass name. Can be used to determine which passes are actually being run
static void pass_exec_callback(void* gcc_data, void* used_data) {
	printf("%s\n", current_pass->name);
};

/// Plugin initialization function, called by gcc to register plugin. Parses plugin arguments
int plugin_init(struct plugin_name_args *plugin_info, struct plugin_gcc_version *version) {
	register_callback(plugin_info->base_name, PLUGIN_INFO, NULL, &plugin_name);

	for (int i = 0; i < plugin_info->argc; i++) {
		//choose dump format based on "dump_format" (short/long/executed) argument
		if (!strcmp(plugin_info->argv[i].key, "dump_format")) {
            register_callback(plugin_info->base_name, PLUGIN_PASS_EXECUTION, pass_exec_callback, NULL);
		}

        if(!strcmp(plugin_info->argv[i].key, "basic_blocks_optimized"))
        {
            struct register_pass_info pass_info;

            pass_info.pass = new gimple_character_pass(g);
            pass_info.reference_pass_name = "local-pure-const";
            pass_info.ref_pass_instance_number = 201;
            pass_info.pos_op = PASS_POS_INSERT_AFTER;

            register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
                                &pass_info);
        }

        if(!strcmp(plugin_info->argv[i].key, "basic_blocks_dfinish"))
        {
            struct register_pass_info pass_info;

            rtl_character_pass* rtl_dump = new rtl_character_pass(g);
            pass_info.pass = rtl_dump;
            rtl_dump->set_dump(true);
            pass_info.reference_pass_name = "dfinish";
            pass_info.ref_pass_instance_number = 1;
            pass_info.pos_op = PASS_POS_INSERT_AFTER;

            register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
                                &pass_info);
        }

		//print help
		if (!strcmp(plugin_info->argv[i].key, "help")) {
			fprintf(stderr, "Plugin implementing gcc phase reorder\n"
							"To pass arguments to plugin use -fplugin-arg-plugin-<name>=<arg>\n"
							"Possible arguments:\n"
							"\tdump_format - short, only pass names. Prints only executed passes\n\n");
		}

    }
	return 0;
}