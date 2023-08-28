//! GCC phase reordering experiment plugin source

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "plugin_passes.h"

#include "callbacks.h"
#include "pass_makers.h"

#include "gcc-plugin.h"

#include "context.h"
#include "function.h"
#include "pass_manager.h"
#include "plugin-version.h"
#include "tree-pass.h"


/// Symbol required by GCC
int plugin_is_GPL_compatible;

extern char *concat(const char *, ...);
extern opt_pass *current_pass;
extern gcc::context *g;

static struct plugin_info plugin_name = {"0.1", "GCC pass reorder experiment"};

/// Inserts marker pass in reference to any pass in pass tree, postfix arg
/// append marker pass name to allow creation of unique passes
static void insert_marker_pass(struct plugin_name_args *plugin_info,
                               enum opt_pass_type type, const char *name,
                               enum pass_positioning_ops pos, int count = 1,
                               const char *postfix = "")
{
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
    opt_pass *dummy_pass_pass = new dummy_pass(dummy_pass_data, g);
    struct register_pass_info dummy_pass_info = {
        dummy_pass_pass,
        name,
        count,
        pos,
    };

    register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
                      &dummy_pass_info);
}

/// Parses pass list file and inserts passes from it into corresponding place
/// in pass tree
static int register_passes_from_file(struct plugin_name_args *plugin_info,
                                     const char *filename)
{
    FILE *passes_file = fopen(filename, "r");
    if (passes_file != NULL) {
        const char *ref_pass_name;
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

        char *line = NULL;
        size_t len = 0;
        int res = getline(&line, &len, passes_file);
        while (res != -1) {
            if (line[res - 1] == '\n') {
                line[res - 1] = 0;
            }
            opt_pass *pass = pass_by_name(line);
            if (pass == NULL) {
                fprintf(stderr,
                        "Failure reading pass file [%s], unknown "
                        "pass name [%s]\n",
                        filename, line);
                return -1;
            }
            if (pass->sub != NULL) {
                fprintf(stderr, "HUH?! [%s]\n", line);
                return -1;
            }
            struct register_pass_info pass_data = {pass, ref_pass_name, 1,
                                                   PASS_POS_INSERT_BEFORE};

            char *subline = NULL;
            size_t sublen = 0;
            res = getline(&subline, &sublen, passes_file);
            opt_pass *prev_sub = NULL;
            while ((res != -1) && (subline[0] == '>')) {
                if (subline[res - 1] == '\n') {
                    subline[res - 1] = 0;
                }
                opt_pass *subpass = pass_by_name(subline + 1);
                if (subpass == NULL) {
                    fprintf(stderr,
                            "Failure reading pass file, "
                            "unknown pass name [%s]\n",
                            subline + 1);
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
            register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP,
                              NULL, &pass_data);
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
static void insert_list_markers(struct plugin_name_args *plugin_info)
{
    insert_marker_pass(plugin_info, opt_pass_type::GIMPLE_PASS, "inline_param",
                       PASS_POS_INSERT_BEFORE, 1, "_list1");
    insert_marker_pass(plugin_info, opt_pass_type::GIMPLE_PASS, "inline_param",
                       PASS_POS_INSERT_AFTER, 56);
    insert_marker_pass(plugin_info, opt_pass_type::GIMPLE_PASS,
                       "*strip_predict_hints", PASS_POS_INSERT_AFTER, 1,
                       "_list2");
    insert_marker_pass(plugin_info, opt_pass_type::GIMPLE_PASS,
                       "local-pure-const", PASS_POS_INSERT_AFTER, 201);
    insert_marker_pass(plugin_info, opt_pass_type::RTL_PASS, "dfinit",
                       PASS_POS_INSERT_AFTER, 1, "_list3");
    insert_marker_pass(plugin_info, opt_pass_type::RTL_PASS, "init-regs",
                       PASS_POS_INSERT_BEFORE, 1);
}

/// Create communication socket
static int create_comm_socket()
{
    int gcc_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (gcc_socket == -1) {
        fprintf(stderr, "plugin failed to create socket\n");
        return -1;
    }

    struct sockaddr_un socket_addr;
    socket_addr.sun_family = AF_UNIX;
    strcpy(socket_addr.sun_path, "gcc_plugin.soc");
    if (bind(gcc_socket, reinterpret_cast<sockaddr *>(&socket_addr),
             sizeof(socket_addr)) == -1) {
        fprintf(stderr, "plugin failed to bind socket, errno [%d]\n", errno);
        return -1;
    }

    return gcc_socket;
}

/// Plugin initialization function, called by gcc to register plugin. Parses
/// plugin arguments
int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version)
{
    register_callback(plugin_info->base_name, PLUGIN_INFO, NULL, &plugin_name);

    for (int i = 0; i < plugin_info->argc; i++) {
        // choose dump format based on "dump_format" (short/long/executed)
        // argument
        if (!strcmp(plugin_info->argv[i].key, "dump_format")) {
            if (!strcmp(plugin_info->argv[i].value, "short")) {
                register_callback(plugin_info->base_name, PLUGIN_OVERRIDE_GATE,
                                  callbacks::pass_dump_short, NULL);
            } else if (!strcmp(plugin_info->argv[i].value, "long")) {
                register_callback(plugin_info->base_name, PLUGIN_OVERRIDE_GATE,
                                  callbacks::pass_dump, NULL);
            } else if (!strcmp(plugin_info->argv[i].value, "executed")) {
                register_callback(plugin_info->base_name, PLUGIN_PASS_EXECUTION,
                                  callbacks::pass_exec, NULL);
            } else if (!strcmp(plugin_info->argv[i].value, "symbols")) {
                struct pass_data name_send_pass_data = {
                    opt_pass_type::GIMPLE_PASS,
                    "func_name_send",
                    OPTGROUP_NONE,
                    TV_NONE,
                    0,
                    0,
                    0,
                    0,
                    0,
                };
                opt_pass *name_send_pass =
                    new func_name_send_pass(name_send_pass_data, g, 0);
                struct register_pass_info name_send_info = {
                    name_send_pass,
                    "*strip_predict_hints",
                    1,
                    PASS_POS_INSERT_BEFORE,
                };

                register_callback(plugin_info->base_name,
                                  PLUGIN_PASS_MANAGER_SETUP, NULL,
                                  &name_send_info);
            } else {
                fprintf(stderr, "Incorrect plugin dump_format value\n");
                return -1;
            }
        }

        // choose pass remove operation
        if (!strcmp(plugin_info->argv[i].key, "pass_remove")) {
            if (!strcmp(plugin_info->argv[i].value, "clear")) {
                insert_marker_pass(plugin_info, GIMPLE_PASS,
                                   "*warn_unused_result",
                                   PASS_POS_INSERT_BEFORE, 1);
                register_callback(plugin_info->base_name, PLUGIN_OVERRIDE_GATE,
                                  callbacks::clear_pass_tree, NULL);
            } else if (!strcmp(plugin_info->argv[i].value, "by_marker")) {
                for (int j = 0; j < plugin_info->argc; j++) {
                    if (!strcmp(plugin_info->argv[j].key, "mark_pass_before")) {
                        char *arg = xstrdup(plugin_info->argv[j].value);
                        char *name = strtok(arg, ".");
                        int type = atoi(strtok(arg, "."));
                        insert_marker_pass(plugin_info,
                                           static_cast<opt_pass_type>(type),
                                           name, PASS_POS_INSERT_BEFORE, 1);
                    }
                    if (!strcmp(plugin_info->argv[j].key, "mark_pass_after")) {
                        char *arg = xstrdup(plugin_info->argv[j].value);
                        char *name = strtok(arg, ".");
                        int type = atoi(strtok(arg, "."));
                        insert_marker_pass(plugin_info,
                                           static_cast<opt_pass_type>(type),
                                           name, PASS_POS_INSERT_AFTER, 1);
                    }
                }
                register_callback(plugin_info->base_name, PLUGIN_OVERRIDE_GATE,
                                  callbacks::clear_pass_tree, NULL);
            } else {
                fprintf(stderr, "Incorrect plugin pass_reorder value\n");
                return -1;
            }
        }

        // choose pass replacement options
        if (!strcmp(plugin_info->argv[i].key, "pass_replace")) {
            // insert required marker passes
            insert_list_markers(plugin_info);
            register_callback(plugin_info->base_name, PLUGIN_OVERRIDE_GATE,
                              callbacks::clear_pass_tree, NULL);

            for (int j = 0; j < plugin_info->argc; j++) {
                if (!strcmp(plugin_info->argv[j].key, "pass_file")) {
                    if (register_passes_from_file(plugin_info,
                                                  plugin_info->argv[j].value)) {
                        return -1;
                    }
                }
            }
        }

        // setup dynamic per-function pass list replacement
        if (!strcmp(plugin_info->argv[i].key, "dyn_replace")) {
            int gcc_socket = create_comm_socket();

            if (gcc_socket == -1) {
                return -1;
            }

            struct sockaddr_un remote_addr;
            remote_addr.sun_family = AF_UNIX;
            bool rem_soc_found = false;
            for (int j = 0; j < plugin_info->argc; j++) {
                if (!strcmp(plugin_info->argv[j].key, "remote_socket")) {
                    strcpy(remote_addr.sun_path, plugin_info->argv[j].value);
                    rem_soc_found = true;
                }
            }

            if (!rem_soc_found) {
                fprintf(stderr,
                        "remote socket for dyn pass replace not specified\n");
                return -1;
            }

            if (connect(gcc_socket, reinterpret_cast<sockaddr *>(&remote_addr),
                        sizeof(remote_addr)) == -1) {
                fprintf(
                    stderr,
                    "plugin failed to connect to remote socket, errno [%d]\n",
                    errno);
                return -1;
            }

            struct pass_data list_insert_pass_data = {
                opt_pass_type::GIMPLE_PASS,
                "dynamic_list_insert",
                OPTGROUP_NONE,
                TV_NONE,
                0,
                0,
                0,
                0,
                0,
            };
            opt_pass *list_insert_pass =
                new list_recv_pass(list_insert_pass_data, g, gcc_socket);
            struct register_pass_info list_insert_info = {
                list_insert_pass,
                "*strip_predict_hints",
                1,
                PASS_POS_INSERT_AFTER,
            };

            register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP,
                              NULL, &list_insert_info);
            insert_marker_pass(plugin_info, opt_pass_type::GIMPLE_PASS,
                               "local-pure-const", PASS_POS_INSERT_AFTER, 201,
                               "_end_list2");
            register_callback(plugin_info->base_name, PLUGIN_FINISH,
                              callbacks::compilation_end, NULL);

            struct pass_data embed_send_pass_data = {
                opt_pass_type::GIMPLE_PASS,
                "embed_send",
                OPTGROUP_NONE,
                TV_NONE,
                0,
                0,
                0,
                0,
                0,
            };
            opt_pass *embed_send_pass =
                new embedding_send_pass(embed_send_pass_data, g, gcc_socket);

            if ((plugin_info->argv[i].value != NULL) &&
                (!strcmp(plugin_info->argv[i].value, "learning"))) {
                // initialize passes for learning
                struct pass_data name_send_pass_data = {
                    opt_pass_type::GIMPLE_PASS,
                    "func_name_send",
                    OPTGROUP_NONE,
                    TV_NONE,
                    0,
                    0,
                    0,
                    0,
                    0,
                };
                opt_pass *name_send_pass =
                    new func_name_send_pass(name_send_pass_data, g, gcc_socket);
                struct register_pass_info name_send_info = {
                    name_send_pass,
                    "dynamic_list_insert",
                    1,
                    PASS_POS_INSERT_BEFORE,
                };

                register_callback(plugin_info->base_name,
                                  PLUGIN_PASS_MANAGER_SETUP, NULL,
                                  &name_send_info);

                struct register_pass_info embed_send_info = {
                    embed_send_pass,
                    "*plugin_dummy_pass_end_list2",
                    1,
                    PASS_POS_INSERT_AFTER,
                };

                register_callback(plugin_info->base_name,
                                  PLUGIN_PASS_MANAGER_SETUP, NULL,
                                  &embed_send_info);
            } else {
                // initialize passes for inference
                struct register_pass_info embed_send_info = {
                    embed_send_pass,
                    "dynamic_list_insert",
                    1,
                    PASS_POS_INSERT_BEFORE,
                };

                register_callback(plugin_info->base_name,
                                  PLUGIN_PASS_MANAGER_SETUP, NULL,
                                  &embed_send_info);
            }
        }

        // print help
        if (!strcmp(plugin_info->argv[i].key, "help")) {
            fprintf(stderr,
                    "Plugin implementing gcc phase reorder\n"
                    "To pass arguments to plugin use "
                    "-fplugin-arg-plugin-<name>=<arg>\n"
                    "Possible arguments:\n"
                    "\tdump_format - choose pass dump format (to "
                    "stdout):\n"
                    "\t\tlong :     provides as much information about "
                    "passes as "
                    "possible. Prints passes even if they did not "
                    "execute\n"
                    "\t\tshort :    short, machine-readable format with "
                    "information "
                    "about pass properties. Prints passes even if they "
                    "did not "
                    "execute\n"
                    "\t\texecuted : short, only pass names. Prints only "
                    "executed "
                    "passes\n\n"
                    "\tpass_remove - remove passes from pass tree:\n"
                    "\t\tclear : clear pass tree (compilation will fail "
                    "with "
                    "internal "
                    "errors)\n"
                    "\t\tby_marker : clear passes between markers, "
                    "requires args "
                    "mark_pass_before/mark_pass_after\n\n"
                    "\tmark_pass_before/mark_pass_after - insert marker "
                    "before/after "
                    "pass, can only inster before/after the first "
                    "instance of "
                    "reference "
                    "pass\n"
                    "\t\treference pass name format is <name>.<type>, "
                    "where type "
                    "is from "
                    "enum {GIMPLE, RTL, SIMPLE_IPA, IPA}\n\n"
                    "\tpass_replace - remove optimization passes and read "
                    "new "
                    "passes to "
                    "insert from lists\n\n"
                    "\tpass_file - specifies files to take passes from. "
                    "Possible "
                    "file "
                    "names are: \"list1.txt\", \"list2.txt\", "
                    "\"list3.txt\".\n"
                    "\t\tPlace where pass list is inserted is based on "
                    "file "
                    "name\n");
        }
    }
    return 0;
}
