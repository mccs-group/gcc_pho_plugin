#pragma once

#include <sys/socket.h>

#include "context.h"
#include "error.h"
#include "gcc-plugin.h"
#include "tree-pass.h"
#include "tree.h"

class dummy_pass : public opt_pass {
  public:
    dummy_pass(const pass_data &data, gcc::context *g) : opt_pass(data, g) {}

    opt_pass *clone() override { return new dummy_pass(*this); }
};

class list_recv_pass : public opt_pass {
  public:
    list_recv_pass(const pass_data &data, gcc::context *g, int socket_fd)
        : opt_pass(data, g), socket_fd{socket_fd},
          input_buf((char *)xmalloc(4096))
    {
    }

    ~list_recv_pass() { free(input_buf); }

    int socket_fd;
    char *input_buf;

    opt_pass *clone() override { return new list_recv_pass(*this); }

    unsigned int execute(function *fun) override;
};

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
