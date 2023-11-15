
#include "rtl_character.hh"


rtl_character::rtl_character()
{
    #define DEF_RTL_EXPR(ENUM, NAME, FORMAT, CLASS)   names.push_back(NAME);
    #include "rtl.def"
    #undef DEF_RTL_EXPR
    inst_count.resize(names.size());
}

void rtl_character::parse_insn(rtx_def* insn)
{
    // std::cout << "inside insn: " << names[GET_CODE(PATTERN(insn))] << std::endl;
    parse_expr(PATTERN(insn));
}

bool rtl_character::bin_instr(enum rtx_class exp_class)
{
    return exp_class == RTX_COMPARE || exp_class == RTX_COMM_COMPARE ||
           exp_class == RTX_COMM_ARITH || exp_class == RTX_BIN_ARITH;
}

bool rtl_character::instr(enum rtx_class exp_class, enum rtx_code exp_code)
{
    return bin_instr(exp_class) || exp_class == RTX_UNARY ||
           exp_class == RTX_TERNARY || exp_class == RTX_BITFIELD_OPS
           || exp_code == USE || exp_code == SET;
}

bool rtl_character::to_count(enum rtx_class exp_class, enum rtx_code exp_code)
{
    return instr(exp_class, exp_code) || exp_class == RTX_CONST_OBJ ||
            exp_code == MEM || exp_code == REG;
}

void rtl_character::count_instr(enum rtx_class exp_class, enum rtx_code exp_code)
{
    if (exp_class == RTX_COMPARE || exp_class == RTX_COMM_COMPARE)
    {
        characteristics[rtl_autophase_embed::ICMP]++;
        return;
    }

    switch(exp_code)
    {
        case rtx_code::PLUS:
            characteristics[rtl_autophase_embed::ADD]++;
            break;
        case rtx_code::SS_PLUS:
        case rtx_code::US_PLUS:
            characteristics[rtl_autophase_embed::ADD]++;
            break;
        case rtx_code::MINUS:
        case rtx_code::SS_MINUS:
        case rtx_code::US_MINUS:
            characteristics[rtl_autophase_embed::SUB]++;
            break;
        case rtx_code::MULT:
        case rtx_code::SS_MULT:
        case rtx_code::US_MULT:
            characteristics[rtl_autophase_embed::MUL]++;
            break;
        case rtx_code::AND:
            characteristics[rtl_autophase_embed::AND]++;
            break;
        case rtx_code::IOR:
            characteristics[rtl_autophase_embed::OR]++;
            break;
        case rtx_code::XOR:
            characteristics[rtl_autophase_embed::XOR]++;
            break;
        case rtx_code::CALL:
            characteristics[rtl_autophase_embed::CALL]++;
            break;
        case rtx_code::RETURN:
            characteristics[rtl_autophase_embed::RET]++;
            break;
        case rtx_code::LSHIFTRT:
            characteristics[rtl_autophase_embed::LSHR]++;
            break;
        case rtx_code::ASHIFTRT:
            characteristics[rtl_autophase_embed::ASHR]++;
            break;
        case rtx_code::TRUNCATE:
        case rtx_code::FLOAT_TRUNCATE:
            characteristics[rtl_autophase_embed::TRUNC]++;
            break;
        case rtx_code::VEC_SELECT:
            characteristics[rtl_autophase_embed::SELECT]++;
            break;
        case rtx_code::SIGN_EXTEND:
            characteristics[rtl_autophase_embed::SEXT]++;
            break;
        case rtx_code::ZERO_EXTEND:
            characteristics[rtl_autophase_embed::ZEXT]++;
            break;
        default:
            break;
    }
}

void rtl_character::safe_statistics(rtx_def* exp)
{
    enum rtx_class exp_class = GET_RTX_CLASS(GET_CODE (exp));
    enum rtx_code exp_code = GET_CODE (exp);

    // std::cout << GET_MODE(exp) << " " << names[exp_code] << std::endl;
    // std::cout << names[exp_code] << std::endl;

    if (instr(exp_class, exp_code))
    {
        count_instr(exp_class, exp_code);
        characteristics[INSTRUCTIONS]++;
        current_bb_inst_amount++;
    }

    if (to_count(exp_class, exp_code))
        inst_count[exp_code]++;

    if (exp_class == RTX_UNARY)
        characteristics[UNARY_INSTR]++;

    if (exp_code == CODE_LABEL)
        characteristics[BRANCHES]++;

    if (exp_code == MEM)
    {
        mem_op = true;
        characteristics[MEMORY_INSTR]++;
    }

    if (bin_instr(exp_class))
        in_binary_instr = true;
    else
        in_binary_instr = false;

    if (in_binary_instr && exp_class == RTX_CONST_OBJ)
        characteristics[BINARY_WITH_CONST]++;

    machine_mode rtx_code = GET_MODE(exp);

    if (rtx_code != machine_mode::VOIDmode)
        last_mode = rtx_code;

    if (exp_code == CONST_INT || exp_code == CONST_WIDE_INT)
    {
        if (last_mode == DImode)
            characteristics[INT64_COUNT]++;

        if (last_mode == SImode)
            characteristics[INT32_COUNT]++;

        if (XINT(exp, 0) == 0)
            characteristics[ZERO_COUNT]++;

        if (XINT(exp, 0) == 1)
            characteristics[ONE_COUNT]++;
    }

}

void rtl_character::parse_expr(rtx_def* exp)
{
    safe_statistics(exp);
    rtx_code exp_code = GET_CODE (exp);
    if (exp_code == SET)
    {
        parse_set_expr(exp);
        return;
    }
    const char *format = GET_RTX_FORMAT (exp_code);
    for (int i = 0; i < GET_RTX_LENGTH(exp_code); i++)
    {
        if (format[i] == 'e')
            parse_expr(XEXP(exp, i));
        if (format[i] == 'E')
            parse_expr_vec(exp, i);

    }
}

void rtl_character::parse_set_expr(rtx_def* exp)
{
    // std::cout << "==========" << std::endl;

    // std::cout << "parse src {" << std::endl;
    mem_op = false;
    parse_expr(XEXP(exp, 0));
    if (mem_op)
    {
        // std::cout << "found store" << std::endl;
        characteristics[STORE]++;
        mem_op = false;
        // std::cout << "deset mem" << std::endl;
    }
    // std::cout << "ended src }" << std::endl;

    // std::cout << "parse dst {" << std::endl;
    parse_expr(XEXP(exp, 1));
    if (mem_op)
    {
        // std::cout << "found load" << std::endl;
        characteristics[LOAD]++;
        mem_op = false;
        // std::cout << "deset mem" << std::endl;
    }
    // std::cout << "ended dst }" << std::endl;

    // std::cout << "==========" << std::endl;

}

void rtl_character::parse_expr_vec(rtx_def* exp, int index)
{
    for (int i = 0; i < XVECLEN(exp, index); i++)
    {
        parse_expr(XVECEXP(exp, index, i));
    }
    return;
}

unsigned int rtl_character::parse_function(function * fun)
{
    basic_block bb;
    rtx_insn* insn;

    // std::cout << names.size();

    FOR_ALL_BB_FN(bb, fun)
    {
        current_bb_inst_amount = 0;
        characteristics[BB_COUNT]++;
        FOR_BB_INSNS(bb, insn)
        {
            // std::cout << names[GET_CODE(insn)] << std::endl;
            if (GET_CODE(insn) == rtx_code::INSN || GET_CODE(insn) == rtx_code::CALL_INSN || GET_CODE(insn) == rtx_code::JUMP_INSN)
                parse_insn(insn);
        }

        if (current_bb_inst_amount >= 15)
            characteristics[BB_MID_INST]++;
        else
            characteristics[BB_LOW_INST]++;

        edge succ_e;
        edge_iterator succ_ei;
        int bb_succ = 0;

        FOR_EACH_EDGE(succ_e, succ_ei, bb->succs)
        {
            bb_succ++;
        }

		edge pred_e;
        edge_iterator pred_ei;
        int bb_pred = 0;

        FOR_EACH_EDGE(pred_e, pred_ei, bb->preds)
        {
            if (EDGE_CRITICAL_P(pred_e))
                characteristics[CRIT_EDGES]++;
            bb_pred++;
            characteristics[EDGES]++;

        }

		switch(bb_pred)
        {
            case 0:
                break;
            case 1:
                characteristics[BB_ONE_PRED]++;
                switch(bb_succ)
                {
                    case 1:
                        characteristics[BB_ONE_PRED_ONE_SUCC]++;
                        break;
                    case 2:
                        characteristics[BB_ONE_PRED_TWO_SUCC]++;
                        break;
                    default:
                        break;
                }
                break;
            case 2:
                characteristics[BB_TWO_PRED]++;
                switch(bb_succ)
                {
                    case 1:
                        characteristics[BB_TWO_PRED_ONE_SUCC]++;
                        break;
                    case 2:
                        characteristics[BB_TWO_EACH]++;
                        break;
                    default:
                        break;
                }
                break;
            default:
                characteristics[BB_MORE_PRED]++;
        }

        switch(bb_succ)
        {
            case 0:
                break;
            case 1:
                characteristics[BB_ONE_SUCC]++;
                break;
            case 2:
                characteristics[BB_TWO_SUCC]++;
                break;
            default:
                break;
        }
    }



    return 0;
}
