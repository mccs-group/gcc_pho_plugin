#include "vf2vec_embed.hh"

void vf2vec_embed::get_proximity_matrix()
{
    double decay = decay_param;
    Eigen::MatrixXd def_use_matrix_power = def_use_matrix;
    proximity_matrix = Eigen::MatrixXd::Zero(def_use_side_size, def_use_side_size);
    for (int i = 0; i < max_path_length; i++)
    {
        proximity_matrix += decay * def_use_matrix_power;
        decay *= decay_param;
        def_use_matrix_power *= def_use_matrix;
    }
}
void vf2vec_embed::get_embed_matrices()
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

    Eigen::MatrixXd D_src = Eigen::MatrixXd::Zero(def_use_side_size, D_matrix_characterisation_len);
    Eigen::MatrixXd D_dst = Eigen::MatrixXd::Zero(def_use_side_size, D_matrix_characterisation_len);

    D_src(Eigen::seqN(0, def_use_side_size), Eigen::seqN(0, D_matrix_characterisation_len)) = U_mat(Eigen::seqN(0, def_use_side_size), Eigen::seqN(0, D_matrix_characterisation_len));
    D_dst(Eigen::seqN(0, def_use_side_size), Eigen::seqN(0, D_matrix_characterisation_len)) = V_mat(Eigen::seqN(0, def_use_side_size), Eigen::seqN(0, D_matrix_characterisation_len));

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

    int elem_count = D_matrix_characterisation_len * def_use_side_size;

    D_src_embed.reserve(elem_count);
    D_dst_embed.reserve(elem_count);

    std::copy(D_src.data(), D_src.data() + elem_count, std::back_inserter(D_src_embed));
    std::copy(D_dst.data(), D_dst.data() + elem_count, std::back_inserter(D_dst_embed));

    std::cout << "==========+=========" << std::endl;

    for (auto* it = D_src_embed.data(); it != D_src_embed.data() + D_src_embed.size(); it++)
        std::cout << *it << " ";
    std::cout << std::endl;

    std::cout << "==========+=========" << std::endl;


    for (auto* it = D_dst_embed.data(); it != D_dst_embed.data() + D_dst_embed.size(); it++)
        std::cout << *it << " ";
    std::cout << std::endl;

    std::cout << "==========+=========" << std::endl;


}

void vf2vec_embed::compress(double* data)
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
    std::cout << Y.size() << std::endl;
    std::copy(Y.data(), Y.data() + D_matrix_characterisation_len, std::back_inserter(val_flow_embed));
}
void vf2vec_embed::reset()
{
    D_src_embed.clear();
    D_dst_embed.clear();
    val_flow_embed.clear();
}