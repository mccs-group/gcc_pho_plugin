#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "gcc-plugin.h"

#include "context.h"
#include "coretypes.h"
#include "errors.h"
#include "function.h"
#include "plugin-version.h"
#include "tree-pass.h"

#include "gimple_character.hh"
#include "pass_makers.h"
#include "plugin_passes.h"

static void delete_pass_tree(opt_pass *pass);

/// This pass clears pass tree till the next nearest
/// '*plugin_dummy_pass_end_list<num>' and fills it with passes received on
/// socket
unsigned int list_recv_pass::execute(function *fun)
{

    int size = recv(socket_fd, input_buf, 4096, 0);
    if (size == -1) {
        internal_error(
            "dynamic replace plugin pass failed to receive data on socket\n");
    }

    char *func_name = input_buf;
    char *pass_list = input_buf + 100;

    if ((func_name[0] != 0) &&
        (strcmp(func_name, IDENTIFIER_POINTER(
                               decl_assembler_name(current_function_decl))))) {
        internal_error(
            "dynamic replace plugin pass received pass list for "
            "wrong function. Expected [%s], got [%s]",
            IDENTIFIER_POINTER(decl_assembler_name(current_function_decl)),
            func_name);
    }

    // Do not clean pass tree if there is nothing to put
    if (pass_list[0] == 0) {
        memset(input_buf, 0, 4096);
        return 0;
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

    if (prev_pass != NULL) {
        prev_pass->next = NULL;
        delete_pass_tree(next);
        next = pass;
    }

    // This allows getting unoptimized embedding in learning mode
    if (pass_list[0] != '?') {
        char *pass_name = strtok(pass_list, "\n");
        while (pass_name) {
            opt_pass *pass_to_insert = pass_by_name(pass_name);
            if (pass_to_insert == NULL) {
                internal_error(
                    "dynamic replace plugin pass received an unknown "
                    "pass name [%s] in function [%s]\n",
                    pass_name, func_name);
            }
            struct register_pass_info pass_data = {pass_to_insert, next->name,
                                                   1, PASS_POS_INSERT_BEFORE};

            char *subpass_name = strtok(NULL, "\n");
            opt_pass *prev_sub = NULL;
            while ((subpass_name != NULL) && (subpass_name[0] == '>')) {
                opt_pass *subpass = pass_by_name(subpass_name + 1);
                if (subpass == NULL) {
                    internal_error(
                        "dynamic replace plugin pass received an unknown "
                        "pass name [%s] in function [%s]\n",
                        pass_name, func_name);
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

    return 0;
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
    int *embedding = autophase_generator.data();
    if (send(socket_fd, embedding,
             autophase_generator.CHARACTERISTICS_AMOUNT * 4, 0) == -1) {
        internal_error("dynamic replace plugin failed to send embedding\n");
    }
    autophase_generator.reset();
    return 0;
}

/// Function to REALLY delete passes. Pass manager will still hold pointers
/// to these passes (and it does not seem fixable without patching GCC itself),
/// but does not use or free()/delete() them,
/// unless -fdump-passes flag is used
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
