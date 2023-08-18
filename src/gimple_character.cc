
#include "gimple_character.hh"
#include "hash-map.h"
#include "hash-traits.h"

#include "gimple-pretty-print.h"

#include <fstream>
#include <sstream>
#include <functional>

static tree count_stmt(gimple_stmt_iterator* i, bool* handled_op, struct walk_stmt_info* wi)
{
    #if DEBUG_PRINTS
    static std::vector<std::string> walk_debug {};

    if (walk_debug.empty())
    {
        #define DEFGSCODE(SYM, STRING, STRUCT)	walk_debug.push_back(STRING);
        #include "gimple.def"
        #undef DEFGSCODE
    }
    #endif
    gimple *gs = gsi_stmt (*i);

    #if DEBUG_PRINTS
    std::cout << "In stmt " << walk_debug[int(gimple_code (gs))] << std::endl;
    #endif

    gimple_character* characterisator = reinterpret_cast<gimple_character*>(wi->info);
    characterisator->parse_stmt(gs);

    return NULL;
}

static tree count_op(tree* node, int* smth, void* wi)
{
    #if DEBUG_PRINTS
    static std::vector<std::string>  tree_debug {};

    if (tree_debug.empty())
    {
        #define DEFTREECODE(SYM, STRING, TYPE, NARGS) tree_debug.push_back(STRING);
        #define END_OF_BASE_TREE_CODES LAST_AND_UNUSED_TREE_CODE,
        #include "all-tree.def"
        #undef DEFTREECODE
        #undef END_OF_BASE_TREE_CODES
    }
    #endif

    #if DEBUG_PRINTS
    enum tree_code code = TREE_CODE(*node);
    enum tree_code_class code_class = tree_code_type[code];

    std::cout << "In op (" << tree_debug[int(code)] << ")";

    if (get_name(*node))
        std::cout << " with name " << get_name(*node);

    std::cout << " with code " << code << std::endl;
    if (code == SSA_NAME)
    {
        std::cout << '_' << SSA_NAME_VERSION (*node) << std::endl;
        // std::cout << "if decl found correctly: " << TREE_CODE((*node)->ssa_name.var);
        std::cout << " & with uid " << DECL_UID((*node)) << std::endl;
    }
    if (code == VAR_DECL)
    {
        std::cout << "var decl" << std::endl;
        std::cout << " & with uid " << DECL_PT_UID((*node)) << std::endl;
    }
    #endif

    gimple_character* characterisator = reinterpret_cast<gimple_character*>((reinterpret_cast<walk_stmt_info*>(wi))->info);
    characterisator->parse_node(*node);

    return NULL;
}


gimple_character::gimple_character()
{
    #if DEBUG_PRINTS

    #define DEFGSCODE(SYM, STRING, STRUCT)	gimple_stmt_names.push_back(STRING);
    #include "gimple.def"
    #undef DEFGSCODE

    #define DEFTREECODE(SYM, STRING, TYPE, NARGS) tree_node_names.push_back(STRING);
    #define END_OF_BASE_TREE_CODES LAST_AND_UNUSED_TREE_CODE,
    #include "all-tree.def"
    #undef DEFTREECODE
    #undef END_OF_BASE_TREE_CODES

    // std::cout << "size " << tree_node_names.size() << std::endl;
    #endif

    walk_info.pset = new hash_set<tree>;
    walk_info.info = this;
    reset();
}

void gimple_character::parse_node(tree node)
{
    if (integer_zerop(node))
        autophase_embeddings[gimple_autophase_embed::ZERO_COUNT]++;
    if (integer_onep(node))
        autophase_embeddings[gimple_autophase_embed::ONE_COUNT]++;

    enum tree_code code = TREE_CODE(node);
    enum tree_code_class code_class = tree_code_type[code];

    save_statistics(code, code_class);
}

void gimple_character::save_statistics(tree_code code, tree_code_class code_class)
{
    if (code_class == tree_code_class::tcc_binary || code_class == tree_code_class::tcc_comparison)
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

        case tree_code::INTEGER_CST:
            autophase_embeddings[gimple_autophase_embed::INT_COUNT]++;
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
        case tree_code::MEM_REF:
        case tree_code::TARGET_MEM_REF:
            autophase_embeddings[gimple_autophase_embed::MEMORY_INSTR]++;
            break;

    }
}

void gimple_character::parse_cond(gimple* gs)
{
    // std::cout << "cond operation: " << tree_node_names[gimple_cond_code(gs)];
    save_statistics(gimple_cond_code(gs), tree_code_type[gimple_cond_code(gs)]);
    // std::cout << " ops: " << tree_node_names[TREE_CODE(gimple_cond_lhs(gs))] << ' ' << tree_node_names[TREE_CODE(gimple_cond_rhs(gs))] << std::endl;
    // parse_gimple_tree(gimple_cond_lhs(gs));
    // parse_gimple_tree(gimple_cond_rhs(gs));

    autophase_embeddings[BRANCHES] += 2;
}

void gimple_character::parse_phi(gimple* gs)
{
    // std::cout << "in phi" << std::endl;
    autophase_embeddings[gimple_autophase_embed::PHI]++;
    autophase_embeddings[gimple_autophase_embed::PHI_TOTAL_ARGS] += gimple_phi_num_args(gs);
    current_bb_phi_args += gimple_phi_num_args(gs);
    current_bb_phi_count++;
}

void gimple_character::parse_call(gimple* gs)
{
    autophase_embeddings[gimple_autophase_embed::CALL]++;
}

void gimple_character::parse_return(gimple* gs)
{
    autophase_embeddings[gimple_autophase_embed::RET]++;
    tree ret_node = gimple_return_retval(static_cast<greturn*>(gs));
    if (ret_node && ((TREE_CODE(ret_node) == tree_code::INTEGER_CST) || (TREE_CODE(TREE_TYPE(ret_node))) == tree_code::INTEGER_TYPE))
    {
        autophase_embeddings[gimple_autophase_embed::RET_INT]++;
    }
}

void gimple_character::parse_assign(gimple* gs)
{

    if (gimple_assign_load_p(gs))
        autophase_embeddings[gimple_autophase_embed::LOAD]++;

    if (gimple_store_p(gs))
        autophase_embeddings[gimple_autophase_embed::STORE]++;

    enum tree_code rhs_code = gimple_assign_rhs_code(gs);
    #if DEBUG_PRINTS
    // if (gimple_assign_load_p(gs))
    // {
    //     std::cout << "load" << std::endl;
    //     print_gimple_stmt(stdout, gs, 0, TDF_SLIM);
    // }
    // if (gimple_store_p(gs))
    // {
    //     std::cout << "store" << std::endl;
    //     print_gimple_stmt(stdout, gs, 0, TDF_SLIM);
    // }

    // if ((rhs_code == MEM_REF) || (rhs_code == TARGET_MEM_REF)
    //     || (TREE_CODE(gimple_assign_lhs(gs)) == MEM_REF) || (TREE_CODE(gimple_assign_lhs(gs)) == TARGET_MEM_REF) )
    //     print_gimple_stmt(stdout, gs, 0, TDF_SLIM);

    #endif


    if (!(get_gimple_rhs_class (rhs_code) == GIMPLE_SINGLE_RHS))
    {
        if (rhs_code == RSHIFT_EXPR)
        {
            if (TYPE_UNSIGNED(TREE_TYPE(gimple_assign_rhs1(gs))))
                autophase_embeddings[gimple_autophase_embed::LSHR]++;
            else
                autophase_embeddings[gimple_autophase_embed::ASHR]++;
        }


        save_statistics(rhs_code, tree_code_type[rhs_code]);
    }
}



void gimple_character::parse_stmt(gimple *gs)
{
    current_instr_count++;

    switch(gimple_code (gs))
    {
        case GIMPLE_COND:
            parse_cond(gs);
            break;

        case GIMPLE_ASSIGN:
            parse_assign(gs);
            break;

        case GIMPLE_CALL:
            parse_call(gs);
            break;

        case GIMPLE_GOTO:
            autophase_embeddings[gimple_autophase_embed::BRANCHES]++;
            autophase_embeddings[gimple_autophase_embed::UNCOND_BRANCHES]++;
            break;

        case GIMPLE_RETURN:
            parse_return(gs);
            break;
    }

    return;
}

void gimple_character::parse_gimple_seq(gimple_seq seq)
{
    gimple_stmt_iterator i;

    for (i = gsi_start (seq); !gsi_end_p (i); gsi_next (&i))
    {
        gimple *gs = gsi_stmt (i);
        autophase_embeddings[gimple_autophase_embed::INSTRUCTIONS]++;

        #if DEBUG_PRINTS
        // enum gimple_code code = gimple_code (gs);
        // std::cout << int(code) << ' ' << gimple_stmt_names[int(code)] << " code" << std::endl;
        #endif

        current_instr_count = 0;

        walk_gimple_stmt(&i, count_stmt, count_op, &walk_info);
        if (in_binary)
            in_binary = false;

        #if DEBUG_PRINTS
        // std::cout << "---parsed stmt---" << std::endl;
        // std::cout << "after walking: " << walk_info.pset->elements() << std::endl;
        #endif

        reset_pset();
    }
}

int* gimple_character::parse_function(function * fun)
{
    basic_block bb;
    // std::cerr << function_name(fun) << std::endl;
    // FILE* fun_file = fopen(function_name(fun), "w");
    FOR_ALL_BB_FN(bb, fun)
    {
        #if DEBUG_PRINTS
        std::cout << "---------bb_" << bb->index << "_start--------" << std::endl;
        #endif
        // gimple_dump_bb(fun_file, bb, 0, TDF_SLIM);

        autophase_embeddings[gimple_autophase_embed::BB_COUNT]++;
        current_bb_phi_args = 0;
        current_bb_phi_count = 0;
        int bb_suc = 0;
        int bb_pred = 0;
        gimple_bb_info *bb_info = &bb->il.gimple;
        gimple_seq seq = bb_info->seq;

        parse_gimple_seq(seq);

        if ((current_instr_count >= 15) && (current_instr_count <= 500))
            autophase_embeddings[gimple_autophase_embed::BB_MID_INST]++;
        else if (current_instr_count < 15)
            autophase_embeddings[gimple_autophase_embed::BB_LOW_INST]++;

        gphi_iterator pi;
        if (!gsi_end_p (gsi_start_phis (bb)))
            autophase_embeddings[gimple_autophase_embed::BB_BEGIN_PHI]++;

        for (pi = gsi_start_phis (bb); !gsi_end_p (pi); gsi_next (&pi))
        {
            parse_phi(pi.phi());
        }

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

        bb_pred = vec_safe_length(bb->preds);

        switch(bb_pred)
        {
            case 0:
                break;
            case 1:
                autophase_embeddings[BB_ONE_PRED]++;
                switch(bb_suc)
                {
                    case 1:
                        autophase_embeddings[BB_ONE_PRED_ONE_SUCC]++;
                        break;
                    case 2:
                        autophase_embeddings[BB_ONE_PRED_TWO_SUCC]++;
                        break;
                    default:
                        break;
                }
                break;
            case 2:
                autophase_embeddings[BB_TWO_PRED]++;
                switch(bb_suc)
                {
                    case 1:
                        autophase_embeddings[BB_TWO_PRED_ONE_SUCC]++;
                        break;
                    case 2:
                        autophase_embeddings[BB_TWO_EACH]++;
                        break;
                    default:
                        break;
                }
                break;
            default:
                autophase_embeddings[BB_MORE_PRED]++;
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
                break;
        }

        #if DEBUG_PRINTS
        std::cout << "---------bb_" << bb->index << "_end--------" << std::endl;
        #endif
    }

    // send_characterisation(fun);
    // reset();
    // fclose(fun_file);

    return autophase_embeddings.data();
}

static bool clear_hash_set(const hash_set<tree>::Key& key, hash_set<tree>* set)
{
    set->remove(key);
    return true;
}

void gimple_character::reset_pset()
{
    walk_info.pset->traverse<hash_set<tree>*, clear_hash_set>(walk_info.pset);
}

void gimple_character::reset()
{
    std::fill(autophase_embeddings.begin(), autophase_embeddings.end(), 0);
}

void gimple_character::send_characterisation(function* fun)
{
    std::stringstream name;
    name << "fun_" << get_name(fun->decl);

    std::ofstream dump_stream(name.str(), std::ios_base::out);
    for (int i = 0; i < autophase_embeddings.size(); i++)
        dump_stream << autophase_embeddings[i] << std::endl;
}