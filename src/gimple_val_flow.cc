#include "gimple_val_flow.hh"

#include <queue>
#include <string_view>

void val_flow_character::set_edge(unsigned def_stmt_id, unsigned use_stmt_id)
{
    if (get_full_embed)
        def_use_matrix(def_stmt_id, use_stmt_id) += 1;
    else
    {
        adjacency_array.push_back(def_stmt_id);
        adjacency_array.push_back(use_stmt_id);
    }
}

static bool aliased_vdef_callback(ao_ref * ref , tree node, void * val_flow_char)
{
    val_flow_character* characteriser = reinterpret_cast<val_flow_character*>(val_flow_char);
    return characteriser->handle_aliased_vdef(ref, node);
}

static tree val_flow_stmt_callback(gimple_stmt_iterator* i, bool* handled_op, struct walk_stmt_info* wi)
{
    *handled_op = true;
    val_flow_character* characteriser = reinterpret_cast<val_flow_character*>(wi->info);
    return characteriser->process_stmt(gsi_stmt (*i));
}

bool val_flow_character::check_call_for_alias(gimple* def_stmt)
{
    #if VAL_FLOW_GET_DEBUG
    std::cout << "analysing call as smth" << std::endl;
    #endif
    tree call_arg;
    for (int i = 0; i < gimple_call_num_args(def_stmt); i++)
    {
        call_arg = gimple_call_arg(def_stmt, i);
        #if VAL_FLOW_GET_DEBUG
        std::cout << "check unchanged:" << std::endl;
        #endif
        if (refs_may_alias_p(current_load_node, call_arg))
        {
            #if VAL_FLOW_GET_DEBUG
            std::cout << "found alias" << std::endl;
            std::cout << "returning " << std::boolalpha << function_ith_arg_def(gimple_call_fndecl(def_stmt), i, 0) << std::endl;
            #endif
            return function_ith_arg_def(gimple_call_fndecl(def_stmt), i, 0);
        }

        for (int call_arg_ref_depth = 1; TREE_CODE(call_arg) == ADDR_EXPR; call_arg_ref_depth++)
        {
            call_arg = TREE_OPERAND(call_arg, 0);
            #if VAL_FLOW_GET_DEBUG
            std::cout << "on arg " << i << " with depth " << call_arg_ref_depth << std::endl;
            if (auto name = get_name(call_arg))
                std::cout << name;
            std::cout << "_" << SSA_NAME_VERSION(call_arg) << " ";
            if (auto current_name = get_name(current_load_node))
                std::cout << current_name;
            
            std::cout << "_" << SSA_NAME_VERSION(current_load_node) << std::endl;
            #endif

            if (refs_may_alias_p(current_load_node, call_arg))
            {
                #if VAL_FLOW_GET_DEBUG
                std::cout << "found alias" << std::endl;
                std::cout << "returning " << std::boolalpha << function_ith_arg_def(gimple_call_fndecl(def_stmt), i, call_arg_ref_depth) << std::endl;
                #endif
                return function_ith_arg_def(gimple_call_fndecl(def_stmt), i, call_arg_ref_depth);
            }
        }
    }
    return false;
}

bool val_flow_character::handle_aliased_vdef(ao_ref* ref , tree node)
{
    #if VAL_FLOW_GET_DEBUG
    if (get_name(node))
        std::cout << "in node " << get_name(node) << SSA_NAME_VERSION(node) << std::endl;
    else
        std::cout << "DUDE, NULLPTR" << std::endl;
    #endif

    gimple *def_stmt = SSA_NAME_DEF_STMT (node);

    if ((def_stmt->code == GIMPLE_CALL) && gimple_call_fndecl(def_stmt))
    {
        if (check_call_for_alias(def_stmt))
        {
            set_edge(def_stmt->uid, current_gs->uid);
            #if VAL_FLOW_GET_DEBUG
            std::cout << "Hey, i am returning true between " << def_stmt->uid << " " << current_gs->uid << std::endl;
            #endif
            return true;
        }
        #if VAL_FLOW_GET_DEBUG
        std::cout << "no luck sir" << std::endl;
        #endif
    }

    if (!gimple_store_p(def_stmt))
        return false;

    tree def_stmt_lhs = gimple_get_lhs(def_stmt);
    if (!def_stmt_lhs)
        return false;

    if (refs_may_alias_p(current_load_node, def_stmt_lhs))
    {
        #if VAL_FLOW_GET_DEBUG
        if (const char* rhs_name = get_name(current_load_node))
            std::cout << rhs_name << SSA_NAME_VERSION(current_load_node) << " which is ";
        std::cout << "rhs of stmt id " << current_gs->uid << " collides with";

        if (const char* lhs_name = get_name(def_stmt_lhs))
            std::cout << " node " << lhs_name << SSA_NAME_VERSION(def_stmt_lhs);
        std::cout << " lhs of stmt id " << def_stmt->uid;

        std::cout << std::endl;
        #endif

        set_edge(def_stmt->uid, current_gs->uid);
        return true;
    }
    else
    {
        #if VAL_FLOW_GET_DEBUG
        if (const char* rhs_name = get_name(current_load_node))
            std::cout << rhs_name << SSA_NAME_VERSION(current_load_node) << " which is ";
        std::cout << "rhs of stmt id " << current_gs->uid << " DOES NOT collide with";

        if (const char* lhs_name = get_name(def_stmt_lhs))
            std::cout << "node " << lhs_name << SSA_NAME_VERSION(def_stmt_lhs);
        std::cout << "lhs of stmt id " << def_stmt->uid;

        std::cout << std::endl;
        #endif
    }

    return false;
}

bool val_flow_character::function_ith_arg_def(tree fun_decl, int arg_index, int arg_ref_depth)
{
    tree arg;
    arg = TYPE_ARG_TYPES (TREE_TYPE(fun_decl));
    for (int i = 0; arg && arg != void_list_node && arg != error_mark_node && (i != arg_index); i++, arg = TREE_CHAIN(arg))
    ;

    if (!arg)
        return false;

    arg = TREE_VALUE(arg);

    for (int i = 0; i < arg_ref_depth; i++, arg = TREE_TYPE(arg))
    ;

    return !(TYPE_QUALS(arg) & TYPE_QUAL_CONST);
}


void val_flow_character::process_call(gimple* gs)
{
    #if VAL_FLOW_GET_DEBUG
    std::cout << "parsing call" << std::endl;
    print_gimple_stmt(stdout, gs, 0, TDF_SLIM);
    #endif
    tree fndecl = gimple_call_fndecl(gs);
    if (fndecl == NULL)
    {
        #if VAL_FLOW_GET_DEBUG
        std::cout << "well, i tried" << std::endl;
        tree fn = gimple_call_fn(gs);
        std::cout << tree_node_names[TREE_CODE(fn)] << std::endl;
        #endif
        return;
    }

    int arg_amount = gimple_call_num_args(gs);

    tree call_arg;
    for (int i = 0; i < arg_amount; i++)
    {
        call_arg = gimple_call_arg(gs, i);
        for (; TREE_CODE(call_arg) == ADDR_EXPR; call_arg = TREE_OPERAND(call_arg, 0))
        ;

        if (!((TREE_CODE(call_arg) == SSA_NAME) || (TREE_CODE(call_arg) == FUNCTION_DECL) || (tree_code_type[TREE_CODE(call_arg)] == tcc_constant)))
        {
            #if VAL_FLOW_GET_DEBUG
            std::cout << "interesting arg# " << i << " : " << tree_node_names[TREE_CODE(call_arg)] << " ";
            #endif
            current_load_node = call_arg;
            init_walk_aliased_vdefs();
        }
    }
}


void val_flow_character::process_assign(gimple* gs)
{
    #if VAL_FLOW_GET_DEBUG
    if (const char* assign_rhs_name = get_name(gimple_assign_rhs1(gs)))
        std::cout << "assign from " << assign_rhs_name;
    std::cout << SSA_NAME_VERSION(gimple_assign_rhs1(gs)) << std::endl;
    #endif

    if (!gimple_assign_load_p(gs))
        return;

    current_load_node = gimple_assign_rhs1(gs);

    init_walk_aliased_vdefs();
}

tree val_flow_character::process_stmt(gimple* gs)
{
    current_gs = gs;

    switch(gs->code)
    {
        case GIMPLE_ASSIGN:
            process_assign(gs);
            break;
        case GIMPLE_CALL:
            process_call(gs);
            break;
        default:
            break;
    }
    return NULL;
}

void val_flow_character::init_walk_aliased_vdefs()
{
    ao_ref refd;

    ao_ref_init (&refd, current_load_node);
    if (auto vuse_node = gimple_vuse (current_gs))
    {
        #if VAL_FLOW_GET_DEBUG
        if (auto vuse_node_name = get_name(vuse_node))
            std::cout << "STMT VUSE is " << vuse_node_name << "_" << SSA_NAME_VERSION(vuse_node) << std::endl;
        else
            std::cout << "STMT got vuse node, but cant get its name" << std::endl;
        std::cout << "STMT VUSE addr " << vuse_node << std::endl;
        #endif

        ::walk_aliased_vdefs (&refd, vuse_node, aliased_vdef_callback, this, NULL);
    }
}

void val_flow_character::remove_virt_op_phi()
{
    while(!phi_nodes_with_virt_op.empty())
    {
        int phi_node_id = phi_nodes_with_virt_op.top();
        phi_nodes_with_virt_op.pop();

        auto&& decrease_lambda = [phi_node_id](int num){ if (num > phi_node_id) num--; return num; };

        std::transform(adjacency_array.begin(), adjacency_array.end(), adjacency_array.begin(), decrease_lambda);
    }
}

void val_flow_character::get_adjacency_array(function* fun)
{
    reset();
    get_full_embed = false;
    stmt_amount = fun->last_stmt_uid;
    adjacency_array.reserve(stmt_amount * 2);
    adjacency_array.push_back(stmt_amount);

    get_val_flow_matrix(fun);
    remove_virt_op_phi();
}

void val_flow_character::parse_function(function * fun)
{
    reset();

    get_full_embed = true;
    stmt_amount = fun->last_stmt_uid;
    def_use_matrix = Eigen::MatrixXd::Zero(stmt_amount, stmt_amount);

    get_val_flow_matrix(fun);
    get_proximity_matrix();
    get_embed_matrices();

    val_flow_embed.reserve(2 * D_matrix_characterisation_len);

    compress(D_src_embed.data());
    compress(D_dst_embed.data());

    #if VAL_FLOW_EMBED_DEBUG
    for (auto&& it : val_flow_embed)
        std::cout << it << " ";
    #endif
}

void val_flow_character::get_val_flow_matrix(function* fun)
{
    basic_block bb;

    FOR_ALL_BB_FN(bb, fun)
    {
        if (bb_unique.find(bb->index) != bb_unique.end())
        {
            std::cerr << "My basic blocks are broken: fun name: " << function_name(fun) << std::endl;
            return;
        }
        else
        {
            bb_unique.insert(bb->index);
        }

        gimple_bb_info *bb_info = &bb->il.gimple;
        gimple_seq seq = bb_info->seq;

        gimple_stmt_iterator i;

        for (i = gsi_start (seq); !gsi_end_p (i); gsi_next (&i))
        {
            gimple* stmt = gsi_stmt (i);
            if (gimple_unique.find(stmt) != gimple_unique.end())
            {
                std::cerr << "My stmts are broken: fun name: " << function_name(fun) << std::endl;
                return;
            }
            else
            {
                gimple_unique.insert(stmt);
            }

            get_stmt_def_use(stmt);
            #if VAL_FLOW_GET_DEBUG
            std::cout << "in " << gimple_stmt_names[gimple_code ((gsi_stmt(i)))] << " id : " << gsi_stmt(i)->uid << std::endl;
            print_gimple_stmt(stdout, gsi_stmt(i), 0, TDF_SLIM);
            #endif
            ::walk_gimple_stmt(&i, val_flow_stmt_callback, nullptr, &walk_info);
            reset_walk_info();
        }

        gphi_iterator pi;

        for (pi = gsi_start_phis (bb); !gsi_end_p (pi); gsi_next (&pi))
        {
            gimple* phi_stmt = pi.phi();
            #if VAL_FLOW_GET_DEBUG
            std::cout << "phi id: " << phi_stmt->uid << std::endl;
            #endif
            if (gimple_unique.find(phi_stmt) != gimple_unique.end())
            {
                std::cerr << "My stmts are broken: fun name: " << function_name(fun) << std::endl;
                return;
            }
            else
            {
                gimple_unique.insert(phi_stmt);
            }

            if (!virtual_operand_p(gimple_phi_result(phi_stmt)))
                get_stmt_def_use(phi_stmt);
            else
                phi_nodes_with_virt_op.push(phi_stmt->uid);
        }
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
    Eigen::BDCSVD<Eigen::MatrixXd> svd(proximity_matrix, Eigen::ComputeFullU | Eigen::ComputeFullV);

    Eigen::MatrixXd U_mat = svd.matrixU();
    Eigen::MatrixXd V_mat = svd.matrixV();

    if (!no_nan_matrix(U_mat.colwise().begin(), U_mat.colwise().end()) || !no_nan_matrix(V_mat.colwise().begin(), V_mat.colwise().end()))
    {
        #if VAL_FLOW_EMBED_DEBUG
        std::cout << "had to apply jac svd" << std::endl;
        #endif
        Eigen::JacobiSVD<Eigen::MatrixXd> jac_svd(proximity_matrix, Eigen::ComputeFullU | Eigen::ComputeFullV);
        U_mat = jac_svd.matrixU();
        V_mat = jac_svd.matrixV();

    }

    embed_vec_len = std::min<long int>(stmt_amount, D_matrix_characterisation_len);
    D_mat_rows = std::max<long int>(stmt_amount, D_matrix_characterisation_len);

    Eigen::MatrixXd D_src = Eigen::MatrixXd::Zero(D_mat_rows, D_matrix_characterisation_len);
    Eigen::MatrixXd D_dst = Eigen::MatrixXd::Zero(D_mat_rows, D_matrix_characterisation_len);

    D_src(Eigen::seqN(0, stmt_amount), Eigen::seqN(0,embed_vec_len)) = U_mat(Eigen::seqN(0, stmt_amount), Eigen::seqN(0,embed_vec_len));
    D_dst(Eigen::seqN(0, stmt_amount), Eigen::seqN(0,embed_vec_len)) = V_mat(Eigen::seqN(0, stmt_amount), Eigen::seqN(0,embed_vec_len));

    #if VAL_FLOW_EMBED_DEBUG
    if (stmt_amount < 20)
    {
        std::cout << "====================" << std::endl;

        std::cout << svd.matrixU() << std::endl;
        std::cout << svd.matrixV() << std::endl;

        std::cout << "==========+=========" << std::endl;

        std::cout << D_src << std::endl;
        std::cout << "and dst:" << std::endl;
        std::cout << D_dst << std::endl;

        std::cout << "==========+=========" << std::endl;
    }
    #endif

    int elem_count = D_matrix_characterisation_len * D_mat_rows;

    D_src_embed.reserve(elem_count);
    D_dst_embed.reserve(elem_count);

    std::copy(D_src.data(), D_src.data() + elem_count, std::back_inserter(D_src_embed));
    std::copy(D_dst.data(), D_dst.data() + elem_count, std::back_inserter(D_dst_embed));

}

void val_flow_character::compress(double* data)
{
    static constexpr int ndim = 1;
    auto Y = qdtsne::initialize_random<ndim>(D_matrix_characterisation_len);

    qdtsne::Tsne<ndim> tSNEcompressor;

    if (3 * qdtsne::Tsne<ndim, double>::Defaults::perplexity >= D_matrix_characterisation_len)
    {
        if (D_matrix_characterisation_len < 6)
            tSNEcompressor.set_perplexity(1);
        else
            tSNEcompressor.set_perplexity(D_matrix_characterisation_len / 3 - 1);
    }
    auto ref = tSNEcompressor.run(data, D_mat_rows, D_matrix_characterisation_len, Y.data());
    std::copy(Y.data(), Y.data() + D_matrix_characterisation_len, std::back_inserter(val_flow_embed));
}


void val_flow_character::get_stmt_def_use(gimple* gs)
{
    ssa_op_iter ssa_iter;
    tree var;

    if (gimple_code (gs) == GIMPLE_PHI)
    {
        get_all_ssa_uses(gimple_phi_result(gs), gs);
        return;
    }

    FOR_EACH_SSA_TREE_OPERAND (var, gs, ssa_iter, SSA_OP_DEF)
    {
        get_all_ssa_uses(var, gs);
    }
}

void val_flow_character::get_all_ssa_uses(tree ssa_name, gimple* gs)
{
    int stmt_id = gs->uid;
    #if VAL_FLOW_GET_DEBUG
    std::cout << "analysing ssa name ";
    if (const char* full_name = get_name(ssa_name))
        std::cout << full_name;
    std::cout << "_" << SSA_NAME_VERSION(ssa_name) << std::endl;
    #endif

    gimple* stmt;
    imm_use_iterator use_it;

    FOR_EACH_IMM_USE_STMT (stmt, use_it, ssa_name)
    {
        set_edge(stmt_id, stmt->uid);
    }
}

void val_flow_character::reset()
{
    gimple_unique.clear();
    bb_unique.clear();
    D_src_embed.clear();
    D_dst_embed.clear();
    val_flow_embed.clear();
    adjacency_array.clear();
}

void val_flow_character::reset_walk_info()
{
    for (auto iter =  walk_info.pset->begin (); iter != walk_info.pset->end (); ++iter)
        walk_info.pset->remove(*iter);
}