#pragma once

namespace callbacks {
void pass_dump(void *gcc_data, void *user_data);
void pass_dump_short(void *gcc_data, void *user_data);
void pass_exec(void *gcc_data, void *used_data);
void clear_pass_tree(void *gcc_data, void *used_data);
} // namespace callbacks
