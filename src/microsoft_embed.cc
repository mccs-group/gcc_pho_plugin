#include "microsoft_embed.hh"

void microsoft_embed::get_transition_matrix()
{
    Eigen::VectorXd mu_vec = Eigen::VectorXd::Zero(adjcency_size);

    auto&& set_one_if_transtion_row_empty = [](auto row_it)
    {
        if (std::all_of(row_it.begin(), row_it.end(), [](double num){return num == 0;}))
            return 1;
        return 0;
    };

    std::transform(adjacency.rowwise().begin(), adjacency.rowwise().end(), mu_vec.begin(), set_one_if_transtion_row_empty);

    #if MICROSOFT_EMBED_DEBUG
    std::cout << mu_vec << std::endl;
    #endif

    double norm_factor = 1.0 / adjcency_size;
    transition_matrix.noalias() = perturbation_factor * (invert_out_degrees * adjacency + norm_factor * mu_vec * Eigen::VectorXd::Constant(adjcency_size, 1).transpose())
                                + (1 - perturbation_factor) * norm_factor * Eigen::MatrixXd::Constant(adjcency_size, adjcency_size, 1);

    #if MICROSOFT_EMBED_DEBUG
    std::cout << transition_matrix << std::endl;
    #endif
}

int microsoft_embed::get_transition_eigen_vector()
{
    Eigen::EigenSolver<Eigen::MatrixXd> eigensolver(transition_matrix.transpose());
    if (eigensolver.info() != Eigen::Success)
        abort();

    #if MICROSOFT_EMBED_DEBUG
    std::cout << eigensolver.eigenvalues() << std::endl;
    std::cout << eigensolver.eigenvectors() << std::endl;
    #endif

    auto eigen_one_finder = [](std::complex<double> z){ return (std::abs(z.imag()) < COMPARISON_PRECISION)
                                                            && (std::abs(z.real() - 1.0) < COMPARISON_PRECISION);};
    auto it = std::find_if(eigensolver.eigenvalues().begin(), eigensolver.eigenvalues().end(), eigen_one_finder);
    int index = std::distance(eigensolver.eigenvalues().begin(), it);

    if (it == eigensolver.eigenvalues().end())
        return -1;

    #if MICROSOFT_EMBED_DEBUG
    // auto&& val = eigensolver.eigenvalues()[index];
    // std::cout << "index " << index << ' ' << val << std::endl;
    #endif

    Eigen::VectorXcd complex_eigen_vec = *(eigensolver.eigenvectors().colwise().begin() + index);
    double coef_sum = std::reduce(complex_eigen_vec.begin(), complex_eigen_vec.end()).real();

    transition_m_eigen_vec = Eigen::VectorXd::Zero(adjcency_size);
    std::transform(complex_eigen_vec.begin(), complex_eigen_vec.end(), transition_m_eigen_vec.begin(), [coef_sum](std::complex<double> z){return z.real() / coef_sum;});

    #if MICROSOFT_EMBED_DEBUG
    std::cout << transition_m_eigen_vec << std::endl;
    #endif

    return 0;
}

void microsoft_embed::get_embed_space()
{
    Eigen::VectorXd Phi_inverse_diag = Eigen::VectorXd::Zero(adjcency_size);
    std::transform(transition_m_eigen_vec.begin(), transition_m_eigen_vec.end(), Phi_inverse_diag.begin(), [](double x){ if (x) return 1 / x; return 0.0;});
    Eigen::MatrixXd Phi_matrix = transition_m_eigen_vec.asDiagonal();
    Eigen::MatrixXd Phi_inverse = Phi_inverse_diag.asDiagonal();

    #if MICROSOFT_EMBED_DEBUG
    std::cout << Phi_matrix << std::endl;
    std::cout << Phi_inverse << std::endl;
    #endif

    Eigen::MatrixXd Phi_inverse_Laplacian = Eigen::MatrixXd::Identity(adjcency_size, adjcency_size) - 0.5 * (transition_matrix +
    Phi_inverse * transition_matrix.transpose() * Phi_matrix);

    Eigen::EigenSolver<Eigen::MatrixXd> eigensolver(Phi_inverse_Laplacian);
    if (eigensolver.info() != Eigen::Success)
        abort();

    #if MICROSOFT_EMBED_DEBUG
    // std::cout << eigensolver.eigenvalues() << std::endl;
    // std::cout << eigensolver.eigenvectors() << std::endl;
    #endif

    std::priority_queue<double, std::vector<double>, std::greater<double>> smallest_eigen;
    std::unordered_map<double, std::vector<int>> eigen_to_indexes_map;

    for (int i = 0; i < eigensolver.eigenvalues().size(); i++)
    {
        double val = eigensolver.eigenvalues()[i].real();
        smallest_eigen.push(val);
        eigen_to_indexes_map[val].push_back(i);
    }

    if (std::abs(smallest_eigen.top()) >= COMPARISON_PRECISION)
        std::cerr << "smth went wrong: could not find zero among eigen values";
    else
        smallest_eigen.pop();

    Eigen::VectorXd projection;
    int embed_vec_count = std::min<int>(one_bb_character_size, smallest_eigen.size());

    smallest_eigen_vec = Eigen::MatrixXd::Zero(adjcency_size, embed_vec_count);

    for (auto&& row_it = smallest_eigen_vec.colwise().begin(); row_it != smallest_eigen_vec.colwise().end();)
    {
        double small_eigen = smallest_eigen.top();
        for (auto&& index : eigen_to_indexes_map[small_eigen])
        {
            Eigen::VectorXcd&& to_project = *(eigensolver.eigenvectors().colwise().begin() + index);
            std::transform(to_project.begin(), to_project.end(), row_it->begin(), [](std::complex<double> z){return z.real();});
            ++row_it;
            if (row_it == smallest_eigen_vec.colwise().end())
                break;
        }
        smallest_eigen.pop();
    }
    #if MICROSOFT_EMBED_DEBUG
    // std::cout << smallest_eigen_vec << std::endl;
    #endif
}

void microsoft_embed::compress_with_tSNE()
{
    static constexpr int ndim = 1;
    auto Y = qdtsne::initialize_random<ndim>(standart_on_bb_char_size); // initial coordinates

    qdtsne::Tsne<ndim> tSNEcompressor;

    if (3 * qdtsne::Tsne<ndim, double>::Defaults::perplexity >= smallest_eigen_vec.cols())
    {
        if (smallest_eigen_vec.cols() < 6)
            tSNEcompressor.set_perplexity(1);
        else
            tSNEcompressor.set_perplexity(smallest_eigen_vec.cols() / 3 - 1);
    }
    auto ref = tSNEcompressor.run(smallest_eigen_vec.data(), smallest_eigen_vec.rows(), standart_on_bb_char_size, Y.data());
    cfg_embedding.reserve(standart_on_bb_char_size);
    std::copy(Y.data(), Y.data() + standart_on_bb_char_size, std::back_inserter(cfg_embedding));
}

void microsoft_embed::reset()
{
    cfg_embedding.clear();
}