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

#include "plugin_passes.h"
#include "pass_makers.h"

/// Symbol required by GCC
int plugin_is_GPL_compatible;

static void delete_pass_tree(opt_pass *pass);

unsigned int list_recv_pass::execute(function *fun)
{
    opt_pass *pass = next;
    opt_pass *prev_pass = NULL;
    while (pass && strcmp(pass->name, "*plugin_dummy_pass_list_end")) {
        prev_pass = pass;
        pass = pass->next;
    }
    if (!pass) {
        error("dynamic replace could not find list end\n");
    }

    prev_pass->next = NULL;
    delete_pass_tree(next);
    next = pass;

    return 0;
}

/// Function to REALLY delete passes. Pass manager will still hold pointers
/// to these passes (and it does not seem fixable without patching GCC itself),
/// but does not use or free()/delete() them,
/// unless -fdump-passes flag is used or toplev::finalize()
/// is called in some other plugin (or newer gcc version)
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
