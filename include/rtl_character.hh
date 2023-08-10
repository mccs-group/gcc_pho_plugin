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


const pass_data rtl_character_data = {
			RTL_PASS,
			"rtl_character",
			OPTGROUP_NONE,
			TV_PLUGIN_INIT,
			0,
			0,
			0,
			0,
			0,
		};

class rtl_character
{
    enum autophase_embed
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
    rtl_character()
    {
        std::cout << "waste" << std::endl;
        test_p = (int *)xcalloc(4, 655360);
        #define DEF_RTL_EXPR(ENUM, NAME, FORMAT, CLASS)   names.push_back(NAME);
        #include "rtl.def"
        #undef DEF_RTL_EXPR
        inst_count.resize(names.size());
    }

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

class rtl_character_pass : public rtl_opt_pass
{
    rtl_character characteriser;

    public:
    rtl_character_pass(gcc::context* g) : rtl_opt_pass(rtl_character_data, g)
    {}
    opt_pass* clone() override {return this;}

    void set_dump(bool flag) {characteriser.to_dump = flag;}

    virtual unsigned int execute (function* fun) override { return characteriser.parse_function(fun); }
};

#endif