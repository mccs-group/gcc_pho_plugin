#ifndef RTL_DUMPER_HH
#define RTL_DUMPER_HH

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <array>

#include "gcc-plugin.h"
#include "tree.h"
#include "tree-pass.h"
#include "rtl.h"

class rtl_character
{
public:
    enum rtl_autophase_embed
    {
        BRANCHES = 0,
        UNARY_INSTR,
        MEMORY_INSTR,
        BINARY_WITH_CONST,
        INSTRUCTIONS,
        FUNCTIONS,
        BB_LESS_15_INST,
        BB_MORE_15_INST,
        BASIC_BLOCKS,
        CRIT_EDGES,
        EDGES,
        INT32_COUNT,
        INT64_COUNT,
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

        BR,
        CALL,
        RET,

        LOAD,
        STORE,

        ICMP,
        SELECT,
        TRUNC,
        SEXT,
        ZEXT,

        CHARACTERISTICS_AMOUNT,
    };

private:
    std::vector<std::string> names;
    std::vector<int> inst_count;
    std::array<int, 3> bb_pred_statistics;
    std::array<int, 3> bb_succ_statistics;
    std::array<int, CHARACTERISTICS_AMOUNT> characteristics = {0};

    bool mem_op = false;
    machine_mode last_mode = machine_mode::VOIDmode;

    int current_bb_inst_amount = 0;
    bool in_binary_instr = false;

    int* test_p = nullptr;

public:
    bool to_dump = false;
    rtl_character();

    void parse_insn(rtx_def* insn);
    void parse_set_expr(rtx_def* exp);
    void parse_expr_vec(rtx_def* exp, int index);
    void parse_expr(rtx_def* exp);

    unsigned int parse_function(function * fun);

    ~rtl_character();

private:
    bool to_count(enum rtx_class exp_class, enum rtx_code exp_code);
    bool instr(enum rtx_class exp_class, enum rtx_code exp_code);
    bool bin_instr(enum rtx_class exp_class);
    void count_instr(enum rtx_class exp_class, enum rtx_code exp_code);
    void safe_statistics(rtx_def* exp);
};



#endif