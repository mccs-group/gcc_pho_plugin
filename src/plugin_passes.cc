#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "cfg_character.hh"
#include "gimple_character.hh"
#include "gimple_val_flow.hh"
#include "pass_makers.h"
#include "plugin_passes.h"

#include "gcc-plugin.h"

#include "context.h"
#include "coretypes.h"
#include "errors.h"
#include "function.h"
#include "options.h"
#include "opts.h"
#include "params.h"
#include "plugin-version.h"
#include "target.h"
#include "tree-pass.h"

static void delete_pass_tree(opt_pass *pass);
extern struct gcc_target targetm;
extern diagnostic_context *global_dc;
extern void maybe_set_param_value(int num, int value, int *params,
                                  int *params_set);
extern int default_param_value(int num);
extern char *xstrdup(const char *src);

/// This pass clears pass tree till the next nearest
/// '*plugin_dummy_pass_end_list<num>' and fills it with passes received on
/// socket
unsigned int list_recv_pass::execute(function *fun)
{
    if (!hot_fun) {
        set_level2_size_opts();
    }

    int size = recv(socket_fd, input_buf, 4096, 0);
    if (size == -1) {
        internal_error(
            "dynamic replace plugin pass failed to receive data on socket\n");
    }

    opt_pass *pass = next;
    opt_pass *prev_pass = NULL;
    const char *dummy_name = "*plugin_dummy_pass_end_list";
    while (pass && strncmp(pass->name, dummy_name, strlen(dummy_name))) {
        prev_pass = pass;
        pass = pass->next;
    }
    if (!pass) {
        internal_error("dynamic replace plugin pass could not find list end\n");
    }

    marker_name = xstrdup(pass->name);

    if (is_inference) {
        inference_recv(pass, prev_pass);
    } else {
        learning_recv(pass, prev_pass);
    }

    return 0;
}

void list_recv_pass::learning_recv(opt_pass *pass, opt_pass *prev_pass)
{
    // If working with initial tree, separate list but preserve passes for
    // future baseline calculations
    if ((base_seq_start == NULL) || (next == base_seq_start)) {
        base_seq_start = next;
        next = pass;
    } else if (prev_pass != NULL) {
        prev_pass->next = NULL;
        delete_pass_tree(next);
        next = pass;
    }

    char *pass_list = input_buf;

    if (pass_list[0] == 0) { // Put initial tree
        next = base_seq_start;
        memset(input_buf, 0, 4096);
    } else if (pass_list[0] != '?') { // This allows getting unoptimized
                                      // embedding in learning mode
        char *pass_name = strtok(pass_list, "\n");
        while (pass_name) {

            if (!strcmp(pass_name, "hot_fun")) {
                set_level2_opts();
                pass_name = strtok(NULL, "\n");
                hot_fun = true;
                continue;
            }

            opt_pass *pass_to_insert = pass_by_name(pass_name);
            if (pass_to_insert == NULL) {
                internal_error(
                    "dynamic replace plugin pass received an unknown "
                    "pass name [%s] in function [%s]\n",
                    pass_name,
                    IDENTIFIER_POINTER(
                        decl_assembler_name(current_function_decl)));
            }
            struct register_pass_info pass_data = {pass_to_insert, marker_name,
                                                   1, PASS_POS_INSERT_BEFORE};

            char *subpass_name = strtok(NULL, "\n");
            opt_pass *prev_sub = NULL;
            while ((subpass_name != NULL) && (subpass_name[0] == '>')) {
                opt_pass *subpass = pass_by_name(subpass_name + 1);
                if (subpass == NULL) {
                    internal_error(
                        "dynamic replace plugin pass received an unknown "
                        "pass name [%s] in function [%s]\n",
                        pass_name,
                        IDENTIFIER_POINTER(
                            decl_assembler_name(current_function_decl)));
                }
                if (pass_to_insert->sub == NULL) {
                    pass_to_insert->sub = subpass;
                } else {
                    prev_sub->next = subpass;
                }
                prev_sub = subpass;

                subpass_name = strtok(NULL, "\n");
            }

            register_callback("dynamic_replace_plugin",
                              PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_data);
            pass_name = subpass_name;
        }
    }

    memset(input_buf, 0, 4096);
}

void list_recv_pass::inference_recv(opt_pass *pass, opt_pass *prev_pass)
{

    opt_pass *new_next = NULL;
    if (subpasses_executed && (input_buf[0] == '>')) {
        new_next = last_sub_pass;
        subpasses_executed = false;
    } else {
        new_next = last_pass ? last_pass : this;
    }
    // Unloop the tree for now, so that GCC can correctly register new
    // passes
    pass->next = next_from_marker;

    if (!recorded_next) {
        next_from_marker = pass->next;
        recorded_next = true;
    }

    if (new_function) {
        if (prev_pass != NULL) {
            prev_pass->next = NULL;
            delete_pass_tree(next);
            next = pass;
        }
        new_function = false;
    }

    next = last_pass ? last_pass : pass;

    char *pass_list = input_buf;

    bool to_unloop = false;
    char *pass_name = strtok(pass_list, "\n");
    while ((pass_name != NULL) && (pass_name[0] == '>')) {
        opt_pass *subpass = pass_by_name(pass_name + 1);
        if (subpass == NULL) {
            internal_error(
                "dynamic replace plugin pass received an unknown "
                "pass name [%s] in function [%s]\n",
                pass_name,
                IDENTIFIER_POINTER(decl_assembler_name(current_function_decl)));
        }
        if (last_sub_pass != NULL) {
            last_sub_pass->next = subpass;
            last_sub_pass = subpass;
        } else {
            internal_error(
                "dynamic replace plugin received pass list that "
                "starts with a "
                "subpass, but no passes for this function (%s) "
                "were provided "
                "earlier\n",
                IDENTIFIER_POINTER(decl_assembler_name(current_function_decl)));
        }
        pass_name = strtok(NULL, "\n");
    }
    while (pass_name) {
        if (!strcmp(pass_name, "plsstop")) {
            to_unloop = true;
            new_function = true;
            last_pass = NULL;
            last_sub_pass = NULL;
            subpasses_executed = false;
            hot_fun = false;
            break;
        }

        if (!strcmp(pass_name, "hot_fun")) {
            set_level2_opts();
            pass_name = strtok(NULL, "\n");
            hot_fun = true;
            continue;
        }

        opt_pass *pass_to_insert = pass_by_name(pass_name);
        if (pass_to_insert == NULL) {
            internal_error(
                "dynamic replace plugin pass received an unknown "
                "pass name [%s] in function [%s]\n",
                pass_name,
                IDENTIFIER_POINTER(decl_assembler_name(current_function_decl)));
        }
        last_sub_pass = NULL;
        struct register_pass_info pass_data = {pass_to_insert, marker_name, 1,
                                               PASS_POS_INSERT_BEFORE};

        char *subpass_name = strtok(NULL, "\n");
        opt_pass *prev_sub = NULL;
        while ((subpass_name != NULL) && (subpass_name[0] == '>')) {
            opt_pass *subpass = pass_by_name(subpass_name + 1);
            if (subpass == NULL) {
                internal_error(
                    "dynamic replace plugin pass received an unknown "
                    "pass name [%s] in function [%s]\n",
                    pass_name,
                    IDENTIFIER_POINTER(
                        decl_assembler_name(current_function_decl)));
            }
            if (pass_to_insert->sub == NULL) {
                pass_to_insert->sub = subpass;
            } else {
                prev_sub->next = subpass;
            }
            prev_sub = subpass;
            last_sub_pass = subpass;

            subpass_name = strtok(NULL, "\n");
        }

        register_callback("dynamic_replace_plugin", PLUGIN_PASS_MANAGER_SETUP,
                          NULL, &pass_data);
        last_pass = pass_to_insert;
        pass_name = subpass_name;
    }

    // If last pass contained subpasses, insert exec_notif_pass to know from
    // which pass to start execution in the next cycle
    if ((last_sub_pass != NULL) && (last_sub_pass->next == NULL)) {
        struct pass_data notif_pass_data = {
            opt_pass_type::GIMPLE_PASS,
            "exec_notif",
            OPTGROUP_NONE,
            TV_PLUGIN_INIT,
            0,
            0,
            0,
            0,
            0,
        };
        opt_pass *notif_pass =
            new exec_notif_pass(notif_pass_data, g, &subpasses_executed);
        notif_pass->next = last_pass->next;
        last_sub_pass->next = notif_pass;
    }

    if (new_next != last_pass) {
        next = new_next->next;
    }

    memset(input_buf, 0, 4096);

    if (!to_unloop) {
        pass->next = cycle_start_pass;
    }
}

/// Function that sets internal compiler parameters as they are for -O2 flag
void list_recv_pass::set_level2_opts() { global_options.x_optimize_size = 0; }

/// Function that sets internal compiler parameters as they are for -Os flag
void list_recv_pass::set_level2_size_opts()
{
    global_options.x_optimize_size = 1;
}

/// This pass only sends current function name (for identification when
/// learning)
unsigned int func_name_send_pass::execute(function *fun)
{
    const char *func_name =
        IDENTIFIER_POINTER(decl_assembler_name(current_function_decl));
    if (socket_fd == 0) {
        const char *dot_index = strchr(func_name, '.');
        if (dot_index != NULL) {
            int len = dot_index - func_name;
            fwrite(func_name, 1, len, stdout);
            printf("\n");
        } else {
            printf("%s\n", func_name);
        }
    } else {
        if (send(socket_fd, func_name, strlen(func_name), 0) == -1) {
            internal_error(
                "dynamic replace plugin failed to send function name\n");
        }
    }
    return 0;
}

/// This pass send current function embedding
unsigned int embedding_send_pass::execute(function *fun)
{

    autophase_generator.parse_function(fun);
    int *autophase_array = autophase_generator.data();
    int autophase_len =
        autophase_generator.CHARACTERISTICS_AMOUNT * sizeof(int);

    cfg_embedding.get_adjacency_array(fun);
    int *cfg_array = cfg_embedding.adjacency_array_data();
    int cfg_len = cfg_embedding.adjacency_array_size() * sizeof(int);

    val_flow_embedding.get_adjacency_array(fun);
    int *val_flow_array = val_flow_embedding.adjacency_array_data();
    int val_flow_len = val_flow_embedding.adjacency_array_size() * sizeof(int);

    int *embedding =
        (int *)xmalloc(autophase_len + cfg_len + val_flow_len + sizeof(int));
    memcpy(embedding, autophase_array, autophase_len);
    memcpy(embedding + autophase_len / sizeof(int) + 1, cfg_array, cfg_len);
    memcpy(embedding + autophase_len / sizeof(int) + cfg_len / sizeof(int) + 1,
           val_flow_array, val_flow_len);
    embedding[47] = cfg_len / sizeof(int);

    if (send(socket_fd, embedding,
             sizeof(int) + autophase_len + cfg_len + val_flow_len, 0) == -1) {
        internal_error("dynamic replace plugin failed to send embedding\n");
    }
    return 0;
}

/// This passes sets x_optimize_size = 1 to make compiler passes operate as they
/// would have with Os flag
unsigned int set_optimize_size_pass::execute(function *fun)
{
    global_options.x_optimize_size = 1;
    return 0;
}

/// Function to REALLY delete passes. Pass manager will still hold pointers
/// to these passes (and it does not seem fixable without patching GCC
/// itself), but does not use or free()/delete() them, unless -fdump-passes
/// flag is used
static void delete_pass_tree(opt_pass *pass)
{
    while (pass) {
        /* Recurse into child passes.  */
        delete_pass_tree(pass->sub);

        opt_pass *next = pass->next;

        /* Delete this pass.  */
        delete pass;

        /* Iterate onto sibling passes.  */
        pass = next;
    }
}
