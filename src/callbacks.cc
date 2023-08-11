#include <stdio.h>
#include <string.h>

#include "gcc-plugin.h"

#include "tree-pass.h"
#include "context.h"
#include "pass_manager.h"

#include "callbacks.h"

extern opt_pass *current_pass;

/// Gate callback, prints as much information about a pass as it can,
/// human-readable
void callbacks::pass_dump(void *gcc_data, void *user_data)
{
    if (current_pass) {
        printf("\n");
        printf("Name: %s\nProperties:\n\tRequired: 0x%x\n\tProvided: "
               "0x%x\n\tDestroyed: 0x%x\n",
               current_pass->name, current_pass->properties_required,
               current_pass->properties_provided,
               current_pass->properties_destroyed);
        printf("Type: %d\n", current_pass->type);
        printf("Gate status: %d\n", *(int *)gcc_data);
        printf("Subpasses: %s", current_pass->sub ? "yes\n" : "no\n");
        printf("TODO before: 0x%X\n", current_pass->todo_flags_start);
        printf("TODO after: 0x%X\n", current_pass->todo_flags_finish);
        printf("Static number %d\n", current_pass->static_pass_number);
    }
}

/// Gate callback, prints pass data in format "name properties_required
/// properties_provided properties_broken"
void callbacks::pass_dump_short(void *gcc_data, void *user_data)
{
    if (current_pass) {
        printf("%s %d %d %d\n", current_pass->name,
               current_pass->properties_required,
               current_pass->properties_provided,
               current_pass->properties_destroyed);
    }
}

/// Gate callback, overrides gate status of passes between markers, disabling
/// them
void callbacks::clear_pass_tree(void *gcc_data, void *used_data)
{
    static bool discard_flag = false;
    if (!strncmp("*plugin_dummy_pass", current_pass->name, 18)) {
        discard_flag = !discard_flag;
        return;
    }
    if (discard_flag) {
        *(bool *)gcc_data = 0;
    }
}

/// Pass execution callback, prints pass name. Can be used to determine which
/// passes are actually being run
void callbacks::pass_exec(void *gcc_data, void *used_data)
{
    printf("%s\n", current_pass->name);
};

/// Clean up after everything is finished
void callbacks::compilation_end(void *gcc_data, void *user_data)
{
    unlink("gcc_plugin.soc");
}
