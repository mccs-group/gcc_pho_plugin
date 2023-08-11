#pragma once

#include "gcc-plugin.h"
#include "plugin-version.h"

int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version);
