#ifndef GIMPLE_DUMPER_HH
#define GIMPLE_DUMPER_HH

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <array>

#include "gcc-plugin.h"

#include "context.h"
#include "function.h"
#include "is-a.h"
#include "predict.h"
#include "basic-block.h"
#include "tree.h"
#include "tree-ssa-alias.h"
#include "gimple-expr.h"
#include "gimple.h"
#include "gimple-ssa.h"
#include "tree-pass.h"
#include "tree-ssa-operands.h"
#include "tree-phinodes.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"

#include "ssa-iterators.h"

// #define DEFGSCODE(SYM, STRING, STRUCT)	walk_debug.push_back(STRING);
// #include "gimple.def"
// #undef DEFGSCODE

const pass_data gimple_character_data =
{
    GIMPLE_PASS,
    "gimple_character",
    OPTGROUP_NONE,
    TV_NONE,
    PROP_gimple_any,
    0,
    0,
    0,
    0
};

class gimple_character
{
    enum gimple_autophase_embed
    {
        BRANCHES = 0,
        UNCOND_BRANCHES,
        UNARY_INSTR,
        MEMORY_INSTR,
        BINARY_WITH_CONST,
        RET_INT,
        INSTRUCTIONS,

        BB_PHI_ARGS_GT_5,
        BB_PHI_ARGS_LESS_5,
        BB_PHI_NO,
        BB_PHI_03,
        BB_PHI_HI,
        BB_BEGIN_PHI,

        BB_ONE_PRED,
        BB_ONE_PRED_ONE_SUCC,
        BB_ONE_PRED_TWO_SUCC,
        BB_ONE_SUCC,
        BB_TWO_PRED,
        BB_TWO_PRED_ONE_SUCC,
        BB_TWO_SUCC,
        BB_TWO_EACH,
        BB_MORE_PRED,

        BB_MID_INST,
        BB_LOW_INST,
        BB_COUNT,

        CRIT_EDGES,
        EDGES,

        INT_COUNT,
        ZERO_COUNT,
        ONE_COUNT,

        SHR,
        SHL,

        ADD,
        SUB,
        MUL,

        AND,
        OR,
        XOR,

        PHI,
        CALL,
        RET,

        LOAD,
        STORE,

        ICMP,
        TRUNC,

        CHARACTERISTICS_AMOUNT,
    };

    std::vector<std::string> gimple_stmt_names;
    std::vector<std::string> tree_node_names;
    std::vector<int> statement_amount;
    std::array<int, CHARACTERISTICS_AMOUNT> autophase_embeddings = {0};

    int current_bb_phi_args = 0;
    int current_bb_phi_count = 0;
    int current_instr_count = 0;
    bool first_stmt = true;

    walk_stmt_info walk_info;
    bool in_binary = false;

public:
    gimple_character()
    {
        #define DEFGSCODE(SYM, STRING, STRUCT)	gimple_stmt_names.push_back(STRING);
        #include "gimple.def"
        #undef DEFGSCODE

        #define DEFTREECODE(SYM, STRING, TYPE, NARGS) tree_node_names.push_back(STRING);
        #define END_OF_BASE_TREE_CODES LAST_AND_UNUSED_TREE_CODE,
        #include "all-tree.def"
        #undef DEFTREECODE
        #undef END_OF_BASE_TREE_CODES

        // std::cout << "size " << tree_node_names.size() << std::endl;
        statement_amount.resize(gimple_stmt_names.size());
    }

    void send_characterisation(function* fun);

    void save_statistics(tree_code code, tree_code_class code_class);
    void parse_tree_code(tree_code code);

    void parse_gimple_seq(gimple_seq seq);

    void parse_assign(gimple* gs);
    void parse_phi(gimple* gs);
    void parse_cond(gimple* gs);
    void parse_call(gimple* gs);
    void parse_return(gimple* gs);

    void parse_stmt(gimple *gs);
    void parse_node(tree node);

    unsigned int parse_function(function * fun);
};

class gimple_character_pass: public gimple_opt_pass
{
    gimple_character characteriser;

public:
    gimple_character_pass(gcc::context* g) : gimple_opt_pass(gimple_character_data, g)
    {}
    opt_pass* clone() override {return this;}

    virtual unsigned int execute (function* fun) override { return characteriser.parse_function(fun); }
};


#endif