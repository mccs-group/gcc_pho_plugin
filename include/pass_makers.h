#pragma once

#include "gcc-plugin.h"

#include "context.h"
#include "tree-pass.h"

extern gcc::context *g;

opt_pass *pass_by_name(const char *name);
