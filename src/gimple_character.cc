
#include "gimple_character.hh"
#include "hash-map.h"
#include "hash-traits.h"

#include "gimple-pretty-print.h"

#include <fstream>
#include <sstream>
#include <functional>

void gimple_character::parse_gimple_tree(tree node)
{
    enum tree_code code = TREE_CODE(node);
    enum tree_code_class code_class = tree_code_type[code];

    std::cout << std::endl;
    if (get_name(node))
        std::cout << "name: " << get_name(node) << ' ';
    std::cout << "code " << code << ' ' << tree_node_names[int(code)] << std::endl;

    tree op0, op1, op2;

    switch (code)
    {
    case COMPONENT_REF:
        parse_gimple_tree(TREE_OPERAND (node, 0));
        return;

    case MEM_REF:
        parse_gimple_tree((TREE_OPERAND (node, 0)));
        parse_gimple_tree((TREE_OPERAND (node, 1)));
        break;

    case ADDR_EXPR:
        parse_gimple_tree(TREE_OPERAND (node, 0));
        break;

    default:
        break;
    }

    save_statistics(code, code_class);
}

void gimple_character::save_statistics(tree_code code, tree_code_class code_class)
{
    if (code_class == tree_code_class::tcc_binary)
        in_binary = true;

    if (in_binary && (code_class == tree_code_class::tcc_constant))
    {
        autophase_embeddings[gimple_autophase_embed::BINARY_WITH_CONST]++;
        in_binary = false;
    }

    if (code_class == tree_code_class::tcc_unary)
    {
        autophase_embeddings[gimple_autophase_embed::UNARY_INSTR]++;
    }

    if (code_class == tree_code_class::tcc_comparison)
    {
        autophase_embeddings[gimple_autophase_embed::ICMP]++;
        return;
    }
    parse_tree_code(code);
}

void gimple_character::parse_tree_code(tree_code code)
{
    switch (code)
    {
        case tree_code::PLUS_EXPR:
        case tree_code::POINTER_PLUS_EXPR:
        case tree_code::REDUC_PLUS_EXPR:
        case tree_code::WIDEN_SUM_EXPR:
            autophase_embeddings[gimple_autophase_embed::ADD]++;
            break;
        case tree_code::MINUS_EXPR:
        case tree_code::SAD_EXPR:
            autophase_embeddings[gimple_autophase_embed::SUB]++;
            break;
        case tree_code::MULT_EXPR:
        case tree_code::MULT_HIGHPART_EXPR:
        case tree_code::DOT_PROD_EXPR:
        case tree_code::WIDEN_MULT_EXPR:
            autophase_embeddings[gimple_autophase_embed::MUL]++;
            break;
        
        case tree_code::BIT_AND_EXPR:
            autophase_embeddings[gimple_autophase_embed::AND]++;
            break;
        case tree_code::BIT_IOR_EXPR:
            autophase_embeddings[gimple_autophase_embed::OR]++;
            break;
        case tree_code::BIT_XOR_EXPR:
            autophase_embeddings[gimple_autophase_embed::XOR]++;
            break;
        case tree_code::TRUNC_DIV_EXPR:
        case tree_code::TRUNC_MOD_EXPR:
        case tree_code::FIX_TRUNC_EXPR:
            autophase_embeddings[gimple_autophase_embed::TRUNC]++;
            break;
        case tree_code::LSHIFT_EXPR:
            autophase_embeddings[gimple_autophase_embed::SHL]++;
            break;
        case tree_code::RSHIFT_EXPR:
            autophase_embeddings[gimple_autophase_embed::SHR]++;
            break;
        case tree_code::MEM_REF:
        case tree_code::TARGET_MEM_REF:
            autophase_embeddings[gimple_autophase_embed::MEMORY_INSTR]++;
            break;

    }
}

void gimple_character::parse_cond(gimple* gs)
{
    save_statistics(gimple_cond_code(gs), tree_code_type[gimple_cond_code(gs)]);
    in_binary = false;
    parse_gimple_tree(gimple_cond_lhs(gs));
    parse_gimple_tree(gimple_cond_rhs(gs));

    autophase_embeddings[BRANCHES] += 2;
}

void gimple_character::parse_phi(gimple* gs)
{
    autophase_embeddings[gimple_autophase_embed::PHI]++;
    current_bb_phi_args += gimple_phi_num_args(gs);
    current_bb_phi_count++;
}

void gimple_character::parse_try(gimple* gs)
{
    parse_gimple_seq(gimple_try_eval(gs));
    parse_gimple_seq(gimple_try_cleanup(gs));
}

void gimple_character::parse_call(gimple* gs)
{
    autophase_embeddings[gimple_autophase_embed::CALL]++;
    tree call_lhs = gimple_call_lhs(gs);

    if (call_lhs)
        parse_gimple_tree(gimple_call_lhs(gs));
}

void gimple_character::parse_return(gimple* gs)
{
    autophase_embeddings[gimple_autophase_embed::RET]++;
    if (gimple_return_retval(static_cast<greturn*>(gs)) &&
        TREE_CODE(gimple_return_retval(static_cast<greturn*>(gs))) == tree_code::INTEGER_CST)
    {
        autophase_embeddings[gimple_autophase_embed::RET_INT]++;
    }
}

void gimple_character::parse_assign(gimple* gs)
{
    std::array<tree, 3> args = {nullptr, nullptr, nullptr};
    switch (gimple_num_ops (gs))
    {
        case 3:
            args[2] = gimple_assign_rhs3 (gs);
        case 2:
            args[1] = gimple_assign_rhs2 (gs);
        case 1:
            args[0] = gimple_assign_rhs1 (gs);
            break;
    }
    enum tree_code lhs_code = TREE_CODE(gimple_assign_lhs(gs));
    std::cout << "lhs : " << tree_node_names[lhs_code] << " rhs code : " << tree_node_names[gimple_assign_rhs_code(gs)] << " with ops: ";

    if (gimple_assign_load_p(gs))
        autophase_embeddings[gimple_autophase_embed::LOAD]++;

    if (gimple_store_p(gs))
        autophase_embeddings[gimple_autophase_embed::STORE]++;

    enum tree_code rhs_code = gimple_assign_rhs_code(gs);
    if (!(get_gimple_rhs_class (rhs_code) == GIMPLE_SINGLE_RHS))
        save_statistics(rhs_code, tree_code_type[rhs_code]);

    for (auto&& it : args)
    {
        if (it)
        {
            std::cout << tree_node_names[TREE_CODE(it)] << ' ';
            parse_gimple_tree(it);
            // save_statistics(TREE_CODE(it), tree_code_type[TREE_CODE(it)]);
        }
    }
    in_binary = false;
    std::cout << std::endl;
}

void gimple_character::parse_asm(gasm* gs)
{
    int inputs_count = gimple_asm_noutputs(gs);
    int outputs_count = gimple_asm_noutputs(gs);

    for (int i = 0; i < inputs_count; i++)
    {
        tree operand = gimple_asm_input_op(gs, i);
        parse_gimple_tree(operand);
    }

    for (int i = 0; i < outputs_count; i++)
    {
        tree operand = gimple_asm_output_op(gs, i);
        parse_gimple_tree(operand);
    }

}

void gimple_character::parse_catch(gcatch* gs)
{
    parse_gimple_seq(gimple_catch_handler(gs));
}

void gimple_character::parse_eh_filt(gimple* gs)
{
    parse_gimple_seq(gimple_eh_filter_failure(gs));
}

void gimple_character::parse_eh_else(geh_else* gs)
{
    parse_gimple_seq(gimple_eh_else_n_body(gs));
    parse_gimple_seq(gimple_eh_else_e_body(gs));
}

void gimple_character::parse_stmt(gimple *gs)
{

}
void gimple_character::parse_op(tree op)
{

}

tree count_stmt(gimple_stmt_iterator* i, bool* handled_op, struct walk_stmt_info* optional)
{
    static std::vector<std::string>  walk_debug {};

    if (walk_debug.empty())
    {
        #define DEFGSCODE(SYM, STRING, STRUCT)	walk_debug.push_back(STRING);
        #include "gimple.def"
        #undef DEFGSCODE
    }
    gimple *gs = gsi_stmt (*i);
    std::cout << "In stmt " << walk_debug[int(gimple_code (gs))] << std::endl;
    // characteriser->parse_stmt(gs);
    return NULL;
}

tree count_op(tree* node, int* smth, void* wi)
{
    static std::vector<std::string>  tree_debug {};

    if (tree_debug.empty())
    {
        #define DEFTREECODE(SYM, STRING, TYPE, NARGS) tree_debug.push_back(STRING);
        #define END_OF_BASE_TREE_CODES LAST_AND_UNUSED_TREE_CODE,
        #include "all-tree.def"
        #undef DEFTREECODE
        #undef END_OF_BASE_TREE_CODES
    }

    std::cout << "In op (" << tree_debug[int(TREE_CODE(*node))] << ")";

    if (get_name(*node))
        std::cout << " with name " << get_name(*node);

    std::cout << " with code " << TREE_CODE(*node) << std::endl;

    // characteriser->parse_op(node);
    return NULL;
}

void gimple_character::parse_gimple_seq(gimple_seq seq)
{
    bool first_stmt = true;
    gimple_stmt_iterator i;

    walk_stmt_info walk_info;
    walk_info.info = this;
    walk_info.pset = new hash_set<tree>;


    for (i = gsi_start (seq); !gsi_end_p (i); gsi_next (&i))
    {
        gimple *gs = gsi_stmt (i);
        enum gimple_code code = gimple_code (gs);
        walk_gimple_stmt(&i, count_stmt, count_op, &walk_info);
        std::cout << std::endl;
        continue;

        statement_amount[int(code)]++;
        autophase_embeddings[gimple_autophase_embed::INSTRUCTIONS]++;
        std::cout << int(code) << ' ' << gimple_stmt_names[int(code)] << " code" << std::endl;

        switch(code)
        {
            case GIMPLE_ASSIGN:
                parse_assign(gs);
                break;

            case GIMPLE_CALL:
                parse_call(gs);
                break;

            case GIMPLE_TRY:
                parse_try(gs);
                break;

            case GIMPLE_PHI:
                if (first_stmt)
                    autophase_embeddings[gimple_autophase_embed::BB_BEGIN_PHI]++;
                parse_phi(gs);
                break;

            case GIMPLE_COND:
                parse_cond(gs);
                break;

            case GIMPLE_GOTO:
                autophase_embeddings[gimple_autophase_embed::BRANCHES]++;
                break;

            case GIMPLE_RETURN:
                parse_return(gs);
                break;

            case GIMPLE_ASM:
                parse_asm(static_cast<gasm*>(gs));

            case GIMPLE_CATCH:
                parse_catch(static_cast<gcatch*>(gs));

            case GIMPLE_EH_FILTER:
                parse_eh_filt(gs);

            case GIMPLE_EH_ELSE:
                parse_eh_else(static_cast<geh_else*>(gs));
        }
        first_stmt = false;

    }
}

unsigned int gimple_character::execute(function * fun)
{
    std::cout << "====== " << get_name(fun->decl)<< " ======" << std::endl;
    // std::cout << get_name(fun->decl) << std::endl;

    autophase_embeddings[gimple_autophase_embed::FUNCTIONS]++;
    basic_block bb;

    FOR_ALL_BB_FN(bb, fun)
    {
        autophase_embeddings[gimple_autophase_embed::BB_COUNT]++;
        current_bb_phi_args = 0;
        current_bb_phi_count = 0;
        int bb_suc = 0;
        int bb_pred = 0;
        gimple_bb_info *bb_info = &bb->il.gimple;
        gimple_seq seq = bb_info->seq;

        parse_gimple_seq(seq);

        switch (current_bb_phi_args)
        {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                autophase_embeddings[BB_PHI_ARGS_LESS_5]++;
                break;
            
            default:
                autophase_embeddings[BB_PHI_ARGS_GT_5]++;
                break;
        }

        switch (current_bb_phi_count)
        {
            case 0:
                autophase_embeddings[BB_PHI_NO]++;
                break;
            case 1:
            case 2:
            case 3:
                autophase_embeddings[BB_PHI_03]++;
                break;
            
            default:
                autophase_embeddings[BB_PHI_HI]++;
                break;
        }

        edge suc_e;
        edge_iterator suc_ei;

        FOR_EACH_EDGE(suc_e, suc_ei, bb->succs)
        {
            if (EDGE_CRITICAL_P(suc_e))
                autophase_embeddings[CRIT_EDGES]++;
            bb_suc++;
            autophase_embeddings[EDGES]++;

        }

        edge pred_e;
        edge_iterator pred_ei;

        FOR_EACH_EDGE(pred_e, pred_ei, bb->succs)
        {
            bb_pred++;
        }

        switch(bb_suc)
        {
            case 0:
                break;
            case 1:
                autophase_embeddings[BB_ONE_SUCC]++;
                break;
            case 2:
                autophase_embeddings[BB_TWO_SUCC]++;
                break;
            default:
                autophase_embeddings[BB_MORE_SUCC]++;
        }

        switch(bb_pred)
        {
            case 0:
                break;
            case 1:
                autophase_embeddings[BB_ONE_PRED]++;
                break;
            case 2:
                autophase_embeddings[BB_TWO_PRED]++;
                break;
            default:
                autophase_embeddings[BB_MORE_PRED]++;
        }
    }

    // send_characterisation(fun);
    std::fill(autophase_embeddings.begin(), autophase_embeddings.end(), 0);

    // std::cout << "============" << std::endl;

    // std::cout << one_succ << ' ' << two_succ << ' ' << multiple_succ << std::endl;
    // std::cout << one_pred << ' ' << two_pred << ' ' << multiple_pred << std::endl;

    return 0;
}

void gimple_character::send_characterisation(function* fun)
{
    std::stringstream name;
    name << "fun_" << get_name(fun->decl) << fun;

    std::ofstream dump_stream(name.str(), std::ios_base::out | std::ios_base::app);
    for (auto&& it : autophase_embeddings)
        dump_stream << it << ' ';
}