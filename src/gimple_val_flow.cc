#include "gimple_val_flow.hh"

#include <queue>
#include <string_view>

void val_flow_character::parse_function(function * fun)
{
    get_val_flow_matrix(fun);
    get_proximity_matrix();
    get_embed_matrices();

    // std::cout << def_use_matrix << std::endl;
    // std::cout << "---------------------------------" << std::endl;
    // std::cout << def_use_matrix * def_use_matrix << std::endl;
}

void val_flow_character::get_val_flow_matrix(function* fun)
{
    stmt_amount = fun->last_stmt_uid;
    def_use_matrix = Eigen::MatrixXd::Zero(stmt_amount, stmt_amount);

    basic_block bb;

    FOR_ALL_BB_FN(bb, fun)
    {
        // std::cout << "---------bb_" << bb->index << "_start--------" << std::endl;

        // gimple_dump_bb(stdout, bb, 0, TDF_SLIM | TDF_ALIAS | TDF_VOPS);

        gimple_bb_info *bb_info = &bb->il.gimple;
        gimple_seq seq = bb_info->seq;

        gimple_stmt_iterator i;

        for (i = gsi_start (seq); !gsi_end_p (i); gsi_next (&i))
        {
            get_stmt_def_use(gsi_stmt (i));
        }

        gphi_iterator pi;

        for (pi = gsi_start_phis (bb); !gsi_end_p (pi); gsi_next (&pi))
        {
            get_stmt_def_use(pi.phi());
        }

        // std::cout << "---------bb_" << bb->index << "_end--------" << std::endl;
    }
}

void val_flow_character::get_proximity_matrix()
{
    double decay = decay_param;
    Eigen::MatrixXd def_use_matrix_power = def_use_matrix;
    proximity_matrix = Eigen::MatrixXd::Zero(def_use_matrix.rows(), def_use_matrix.cols());
    for (int i = 0; i < max_path_length; i++)
    {
        proximity_matrix += decay * def_use_matrix_power;
        decay *= decay_param;
        def_use_matrix_power *= def_use_matrix;
    }
}

void val_flow_character::get_embed_matrices()
{
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(proximity_matrix, Eigen::ComputeFullU | Eigen::ComputeFullV);

    int embed_vec_len = std::min<long int>(characterisation_len, proximity_matrix.cols());

    auto matrix_U_begin = svd.matrixU().colwise().begin();
    auto matrix_V_begin = svd.matrixU().colwise().begin();

    D_src_embed.reserve(embed_vec_len * stmt_amount);
    D_dst_embed.reserve(embed_vec_len * stmt_amount);

    std::copy(svd.matrixU().data(), svd.matrixU().data() + embed_vec_len * stmt_amount, std::back_inserter(D_src_embed));
    std::copy(svd.matrixV().data(), svd.matrixV().data() + embed_vec_len * stmt_amount, std::back_inserter(D_dst_embed));

    // std::cout << svd.matrixU() << std::endl;
    // std::cout << "---------------------" << std::endl;
    // std::cout << svd.matrixV() << std::endl;

}

void val_flow_character::get_stmt_def_use(gimple* gs)
{
    ssa_op_iter ssa_iter;
    tree var;

    // if (POINTER_TYPE_P (TREE_TYPE (node)) && SSA_NAME_PTR_INFO (node))
    // {
    //     struct ptr_info_def *pi = SSA_NAME_PTR_INFO (node);
    // }
    // std::cout << "in " << gimple_stmt_names[gimple_code (gs)] << " id : " << gs->uid << std::endl;

    if (gimple_code (gs) == GIMPLE_PHI)
    {
        get_all_ssa_uses(gimple_phi_result(gs), gs);
        // print_ssa_info(gimple_phi_result(gs));
        return;
    }

    FOR_EACH_SSA_TREE_OPERAND (var, gs, ssa_iter, SSA_OP_ALL_DEFS)
    {
        get_all_ssa_uses(var, gs);
        // print_ssa_info(var);
    }
}

void val_flow_character::get_all_ssa_uses(tree ssa_name, gimple* gs)
{
    int stmt_id = gs->uid;
    // std::cout << "analysing ssa name ";
    // if (const char* full_name = get_name(ssa_name))
    //     std::cout << full_name;
    // std::cout << "_" << SSA_NAME_VERSION(ssa_name) << std::endl;

    gimple* stmt;
    imm_use_iterator use_it;

    FOR_EACH_IMM_USE_STMT (stmt, use_it, ssa_name)
    {
        def_use_matrix(stmt_id, stmt->uid) += 1;
    }
}

void val_flow_character::print_ssa_info(tree var)
{
    const char* ssa_var_name = get_name(var);
    std::cout << "let me tell you smth about ";
    print_generic_expr (stdout, var, TDF_SLIM);
    std::cout << std::endl;

    if (var->ssa_name.var)
        std::cout << " decl uid: " << DECL_PT_UID(var->ssa_name.var) << std::endl;
    if (POINTER_TYPE_P (TREE_TYPE (var)))
    {
        if (SSA_NAME_PTR_INFO (var))
        {
            std::cout << "info:" << std::endl;
            dump_ssaname_info_to_file(stdout, var, 0);
        }
        else
        {
            std::cout << "pointer, but info not available" << std::endl;
        }
    }
    else
    {
        std::cout << "not pointer" << std::endl;
    }
    imm_use_iterator use_it;

    gimple* def_stmt = SSA_NAME_DEF_STMT(var);
    if (def_stmt)
        std::cout << "defined by " << gimple_stmt_names[gimple_code (def_stmt)] << " id : " << def_stmt->uid << std::endl;
    else
        std::cout << "could not find def" << std::endl;

    gimple* stmt;
    FOR_EACH_IMM_USE_STMT (stmt, use_it, var)
    {
        std::cout << "user of ";
        if (ssa_var_name)
            std::cout << ssa_var_name;
        std::cout << '_' << SSA_NAME_VERSION(var) << " is " << gimple_stmt_names[(gimple_code (stmt))] << " id : " << stmt->uid << std::endl;
    }
    std::cout << std::endl;
}

static bool clear_hash_set(const hash_set<tree>::Key& key, hash_set<tree>* set)
{
    set->remove(key);
    return true;
}

void val_flow_character::reset_walk_info()
{
    walk_info.pset->traverse<hash_set<tree>*, clear_hash_set>(walk_info.pset);
}