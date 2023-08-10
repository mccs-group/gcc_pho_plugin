#pragma once

#include "gcc-plugin.h"
#include "context.h"
#include "error.h"
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
        : opt_pass(data, g), socket_fd{socket_fd}
    {
    }

    int socket_fd;

    opt_pass *clone() override { return new list_recv_pass(*this); }

    unsigned int execute(function *fun) override;
};
