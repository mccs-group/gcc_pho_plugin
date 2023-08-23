#include "gimple_val_flow.hh"

#include <queue>
#include <string_view>

static
bool check_collision(ao_ref * ref , tree node, void * val_flow_char)
{
    val_flow_character* characteriser = reinterpret_cast<val_flow_character*>(val_flow_char);
    return characteriser->handle_aliased_vdef(ref, node);
}

static tree val_flow_stmt_callback(gimple_stmt_iterator* i, bool* handled_op, struct walk_stmt_info* wi)
{
    val_flow_character* characteriser = reinterpret_cast<val_flow_character*>(wi->info);
    return characteriser->handle_stmt(gsi_stmt (*i), handled_op);
}

bool val_flow_character::handle_aliased_vdef(ao_ref* ref , tree node)
{
    #if VAL_FLOW_DEBUG
    if (get_name(node))
        std::cout << "in node " << get_name(node) << SSA_NAME_VERSION(node) << std::endl;
    else
        std::cout << "DUDE, NULLPTR" << std::endl;
    #endif

    gimple *def_stmt = SSA_NAME_DEF_STMT (node);
    if (!gimple_store_p(def_stmt))
    {
        return false;
    }

    // std::cout << "stmt with id " << def_stmt->uid << " is a store" << std::endl;

    tree def_stmt_lhs = gimple_get_lhs(def_stmt);
    if (!def_stmt_lhs)
        return false;

    if (refs_may_alias_p(current_load_node, def_stmt_lhs))
    {
        #if VAL_FLOW_DEBUG
        if (const char* rhs_name = get_name(current_load_node))
            std::cout << rhs_name << SSA_NAME_VERSION(current_load_node) << " which is ";
        std::cout << "rhs of stmt id " << current_gs->uid << " collides with";

        if (const char* lhs_name = get_name(def_stmt_lhs))
            std::cout << " node " << lhs_name << SSA_NAME_VERSION(def_stmt_lhs);
        std::cout << " lhs of stmt id " << def_stmt->uid;

        std::cout << std::endl;
        #endif

        if (get_full_embed)
            def_use_matrix(def_stmt->uid, current_gs->uid) += 1;
        else
        {
            adjacency_arr.push_back(def_stmt->uid);
            adjacency_arr.push_back(current_gs->uid);
        }
        return true;
    }
    else
    {
        #if VAL_FLOW_DEBUG
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

tree val_flow_character::handle_stmt(gimple* gs,  bool* handled_op)
{
    *handled_op = true;

    current_gs = gs;

    if (gs->code != GIMPLE_ASSIGN)
    {
        return NULL;
    }
    #if VAL_FLOW_DEBUG
    else
    {
        if (const char* assign_rhs_name = get_name(gimple_assign_rhs1(gs)))
            std::cout << "assign from " << assign_rhs_name;
        std::cout << SSA_NAME_VERSION(gimple_assign_rhs1(gs)) << std::endl;
    }
    #endif

    current_load_node = gimple_assign_rhs1(gs);

    if (!gimple_assign_load_p(gs))
    {
        return NULL;
    }

    walk_aliased_vdefs(current_load_node);

    return NULL;
}

void val_flow_character::walk_aliased_vdefs(tree node)
{
    ao_ref refd;

    ao_ref_init (&refd, current_load_node);
    if (auto vuse_node = gimple_vuse (current_gs))
    {
        #if VAL_FLOW_DEBUG
        if (auto vuse_node_name = get_name(vuse_node))
            std::cout << "STMT VUSE is " << vuse_node_name << "_" << SSA_NAME_VERSION(vuse_node) << std::endl;
        else
            std::cout << "STMT got vuse node, but cant get its name" << std::endl;
        std::cout << "STMT VUSE addr " << vuse_node << std::endl;
        #endif

        ::walk_aliased_vdefs (&refd, vuse_node, check_collision, this, NULL);
    }
}

int* val_flow_character::get_adjacency_array(function* fun)
{
    reset();
    get_full_embed = false;
    adjacency_arr.reserve(stmt_amount * 2);
    get_val_flow_matrix(fun);

    return adjacency_arr.data();
}

void val_flow_character::parse_function(function * fun)
{
    reset();
    // if (fun->last_stmt_uid <= 3)
    //     return;
    stmt_amount = fun->last_stmt_uid;
    def_use_matrix = Eigen::MatrixXd::Zero(stmt_amount, stmt_amount);
    get_val_flow_matrix(fun);
    get_proximity_matrix();
    get_embed_matrices();

    val_flow_embed.reserve(2 * D_matrix_characterisation_len);

    compress(D_src_embed.data());
    compress(D_dst_embed.data());

    // for (auto&& it : val_flow_embed)
    //     std::cout << it << " ";
}

void val_flow_character::get_val_flow_matrix(function* fun)
{
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
            #if VAL_FLOW_DEBUG
            std::cout << "in " << gimple_stmt_names[gimple_code ((gsi_stmt(i)))] << " id : " << gsi_stmt(i)->uid << std::endl;
            print_gimple_stmt(stdout, gsi_stmt(i), 0, TDF_SLIM);
            #endif

            walk_gimple_stmt(&i, val_flow_stmt_callback, nullptr, &walk_info);
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
    // std::cout << def_use_matrix << std::endl;
    Eigen::BDCSVD<Eigen::MatrixXd> svd(proximity_matrix, Eigen::ComputeFullU | Eigen::ComputeFullV);

    embed_vec_len = std::min<long int>(stmt_amount, D_matrix_characterisation_len);
    D_mat_rows = std::max<long int>(stmt_amount, D_matrix_characterisation_len);

    Eigen::MatrixXd D_src = Eigen::MatrixXd::Zero(D_mat_rows, D_matrix_characterisation_len);
    Eigen::MatrixXd D_dst = Eigen::MatrixXd::Zero(D_mat_rows, D_matrix_characterisation_len);

    D_src(Eigen::seqN(0, stmt_amount), Eigen::seqN(0,embed_vec_len)) = svd.matrixU()(Eigen::seqN(0, stmt_amount), Eigen::seqN(0,embed_vec_len));
    D_dst(Eigen::seqN(0, stmt_amount), Eigen::seqN(0,embed_vec_len)) = svd.matrixV()(Eigen::seqN(0, stmt_amount), Eigen::seqN(0,embed_vec_len));

    // if (stmt_amount < 20)
    // {
    //     std::cout << "====================" << std::endl;

    //     std::cout << svd.matrixU() << std::endl;
    //     std::cout << svd.matrixV() << std::endl;

    //     std::cout << "==========+=========" << std::endl;

    //     std::cout << D_src << std::endl;
    //     std::cout << "and dst:" << std::endl;
    //     std::cout << D_dst << std::endl;

    //     std::cout << "==========+=========" << std::endl;
    // }

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
        // std::cout << "perplex: " << (D_matrix_characterisation_len / 3 - 1) << std::endl;
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

    FOR_EACH_SSA_TREE_OPERAND (var, gs, ssa_iter, SSA_OP_ALL_DEFS)
    {
        get_all_ssa_uses(var, gs);
    }
}

void val_flow_character::get_all_ssa_uses(tree ssa_name, gimple* gs)
{
    int stmt_id = gs->uid;
    #if VAL_FLOW_DEBUG
    std::cout << "analysing ssa name ";
    if (const char* full_name = get_name(ssa_name))
        std::cout << full_name;
    std::cout << "_" << SSA_NAME_VERSION(ssa_name) << std::endl;
    #endif

    gimple* stmt;
    imm_use_iterator use_it;

    FOR_EACH_IMM_USE_STMT (stmt, use_it, ssa_name)
    {
        if(get_full_embed)
            def_use_matrix(stmt_id, stmt->uid) += 1;
        else
        {
            adjacency_arr.push_back(stmt_id);
            adjacency_arr.push_back(stmt->uid);
        }
    }
}

void val_flow_character::reset()
{
    D_src_embed.clear();
    D_dst_embed.clear();
    val_flow_embed.clear();
    adjacency_arr.clear();
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