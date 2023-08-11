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

#define DEBUG_PRINTS 0

// #define DEFGSCODE(SYM, STRING, STRUCT)	walk_debug.push_back(STRING);
// #include "gimple.def"
// #undef DEFGSCODE

class gimple_character
{
public:
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
        PHI_TOTAL_ARGS,
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

        ASHR,
        LSHR,
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

private:

    #if DEBUG_PRINTS
    std::vector<std::string> gimple_stmt_names;
    std::vector<std::string> tree_node_names;
    #endif

    std::array<int, CHARACTERISTICS_AMOUNT> autophase_embeddings = {0};

    int current_bb_phi_args = 0;
    int current_bb_phi_count = 0;
    int current_instr_count = 0;
    bool first_stmt = true;

    walk_stmt_info walk_info;
    bool in_binary = false;
    bool in_rshisft = false;

public:
    gimple_character();

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

    void get_stmt_def_use(gimple* gs);

    void reset();
    void reset_pset();

    int* data() { return autophase_embeddings.data(); }

    int* parse_function(function * fun);
};

#endif