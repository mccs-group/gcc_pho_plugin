#pragma once

#include <sys/socket.h>

#include "cfg_character.hh"
#include "gimple_character.hh"
#include "gimple_val_flow.hh"

#include "context.h"
#include "error.h"
#include "gcc-plugin.h"
#include "opts.h"
#include "target.h"
#include "tree-pass.h"
#include "tree.h"

/// Dummy marker pass, does nothing
class dummy_pass : public opt_pass {
  public:
    dummy_pass(const pass_data &data, gcc::context *g) : opt_pass(data, g) {}

    opt_pass *clone() override { return new dummy_pass(*this); }
};

/// Pass that signalizes that it was executed. Used to determine whether or not
/// subpasses were executed
class exec_notif_pass : public opt_pass {
  public:
    exec_notif_pass(const pass_data &data, gcc::context *g, bool *target_field)
        : opt_pass(data, g), target_field(target_field)
    {
    }

    bool *target_field;

    opt_pass *clone() override { return new exec_notif_pass(*this); }

    unsigned int execute(function *fun) override
    {
        *target_field = true;
        return 0;
    }
};

/// Pass that receives pass list on socket and inserts it
class list_recv_pass : public opt_pass {
  public:
    list_recv_pass(const pass_data &data, gcc::context *g, int socket_fd)
        : opt_pass(data, g), socket_fd{socket_fd},
          input_buf((char *)xcalloc(4096, 1)), base_seq_start(NULL)
    {
        set_default_handlers(&handlers, targetm.target_option.override);
    }

    ~list_recv_pass() { free(input_buf); }

    int socket_fd;
    char *input_buf;
    char *marker_name;
    opt_pass *base_seq_start;
    opt_pass *cycle_start_pass = NULL;
    opt_pass *next_from_marker = NULL;
    bool is_inference = false;
    bool new_function = true;
    bool recorded_next = false;
    bool subpasses_executed = false;
    bool hot_fun = false;
    opt_pass *last_pass = NULL;
    opt_pass *last_sub_pass = NULL;
    struct cl_option_handlers handlers;

    opt_pass *clone() override { return new list_recv_pass(*this); }

    unsigned int execute(function *fun) override;
    void set_level2_opts();
    void set_level2_size_opts();
    void learning_recv(opt_pass *pass, opt_pass *prev_pass);
    void inference_recv(opt_pass *pass, opt_pass *prev_pass);
};

/// Pass that sends current function name
class func_name_send_pass : public opt_pass {
  public:
    func_name_send_pass(const pass_data &data, gcc::context *g, int socket_fd)
        : opt_pass(data, g), socket_fd{socket_fd}
    {
    }

    int socket_fd;

    opt_pass *clone() override { return new func_name_send_pass(*this); }

    unsigned int execute(function *fun) override;
};

/// Pass that sends current function embedding
class embedding_send_pass : public opt_pass {
  public:
    embedding_send_pass(const pass_data &data, gcc::context *g, int socket_fd)
        : opt_pass(data, g), socket_fd{socket_fd}
    {
    }

    int socket_fd;
    gimple_character autophase_generator;
    cfg_character cfg_embedding;
    val_flow_character val_flow_embedding;

    opt_pass *clone() override { return new embedding_send_pass(*this); }

    unsigned int execute(function *fun) override;
};
