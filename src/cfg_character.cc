#include "cfg_character.hh"

#include <numeric>
#include <iostream>
#include <queue>
#include <unordered_map>


int cfg_character::get_cfg_embed(function * fun)
{
    if ((fun->cfg->x_n_basic_blocks - service_bb_amount) <= 3)
        return 0;

    get_adjacency_matrix(fun);
    get_transition_matrix(fun);
    if (get_transition_eigen_vector())
    {
        std::cerr << "Fatal, could not find eigen value = 1 on function " << function_name(fun) << std::endl;
        return -1;
    }
    get_embed_space();
    compress_with_tSNE();

    return 0;
}

void cfg_character::get_adjacency_matrix(function * fun)
{
    bb_amount = fun->cfg->x_n_basic_blocks - service_bb_amount; // special empty basic blocks with id 0 and 1 which are entry/exit are not interesting

    adjacency = Eigen::MatrixXd::Zero(bb_amount, bb_amount);
    invert_out_degrees = Eigen::MatrixXd::Zero(bb_amount, bb_amount);

    basic_block bb_for_matrix;
    FOR_ALL_BB_FN(bb_for_matrix, fun)
    {
        // gimple_dump_bb(stdout, bb_for_matrix, 0, TDF_SLIM);
        int current_id = bb_for_matrix->index;
        if (current_id == 0 || current_id == 1)
            continue;

        current_id -= service_bb_amount;

        // invert_out_degrees(current_id, current_id) = 1 / vec_safe_length(bb_for_matrix->preds); // Damn boy where did you find this

        edge suc_e;
        edge_iterator suc_ei;
        int suc_amount = 0;
        FOR_EACH_EDGE(suc_e, suc_ei, bb_for_matrix->succs)
        {
            if (suc_e->dest->index == 1)
                continue;
            
            suc_amount += 1;
            int suc_index = suc_e->dest->index - service_bb_amount;
            adjacency(current_id, suc_index) = 1;
        }
        if (suc_amount)
            invert_out_degrees(current_id, current_id) = 1.0 / suc_amount;
    }

    // std::cout << adjacency << std::endl;
    // std::cout << invert_out_degrees << std::endl;

}

void cfg_character::get_transition_matrix(function * fun)
{
    Eigen::VectorXd mu_vec = Eigen::VectorXd::Zero(bb_amount);
    int i = 0;

    auto&& set_one_if_transtion_row_empty = [](auto row_it)
    {
        if (std::all_of(row_it.begin(), row_it.end(), [](double num){return num == 0;}))
            return 1;
        return 0;
    };

    std::transform(adjacency.rowwise().begin(), adjacency.rowwise().end(), mu_vec.begin(), set_one_if_transtion_row_empty);

    // for (auto&& it : mu_vec)
    //     std::cout << it << ' ';

    double norm_factor = 1.0 / bb_amount;
    transition_matrix.noalias() = perturbation_factor * (invert_out_degrees * adjacency + norm_factor * mu_vec * Eigen::VectorXd::Constant(bb_amount, 1).transpose())
                                + (1 - perturbation_factor) * norm_factor * Eigen::MatrixXd::Constant(bb_amount, bb_amount, 1);

    // std::cout << 'P' << std::endl;
    // std::cout << transition_matrix << std::endl;
}

int cfg_character::get_transition_eigen_vector()
{
    // std::cout << "to solve:" << std::endl;
    // std::cout << transition_matrix.transpose() << std::endl;

    Eigen::EigenSolver<Eigen::MatrixXd> eigensolver(transition_matrix.transpose());
    if (eigensolver.info() != Eigen::Success)
        abort();

    // std::cout << "The eigenvalues of A are:\n" << eigensolver.eigenvalues() << std::endl;
    // std::cout << "Here's a matrix whose columns are eigenvectors of A \n"
    //     << "corresponding to these eigenvalues:\n"
    //     << eigensolver.eigenvectors() << std::endl;


    auto eigen_one_finder = [](std::complex<double> z){ return (std::abs(z.imag()) < COMPARISON_PRECISION)
                                                            && (std::abs(z.real() - 1.0) < COMPARISON_PRECISION);};
    auto it = std::find_if(eigensolver.eigenvalues().begin(), eigensolver.eigenvalues().end(), eigen_one_finder);
    int index = std::distance(eigensolver.eigenvalues().begin(), it);

    if (it == eigensolver.eigenvalues().end())
        return -1;

    // auto&& val = eigensolver.eigenvalues()[index];
    // std::cout << "index " << index << ' ' << val << std::endl;
    // std::cout << "vec" << std::endl << *(eigensolver.eigenvectors().colwise().begin()) << std::endl;
    // std::cout << "mult with matrix" << std::endl;
    // std::cout << (to_solve * *(eigensolver.eigenvectors().colwise().begin()));

    Eigen::VectorXcd complex_eigen_vec = *(eigensolver.eigenvectors().colwise().begin() + index);
    double coef_sum = std::reduce(complex_eigen_vec.begin(), complex_eigen_vec.end()).real();

    transition_m_eigen_vec = Eigen::VectorXd::Zero(bb_amount);
    std::transform(complex_eigen_vec.begin(), complex_eigen_vec.end(), transition_m_eigen_vec.begin(), [coef_sum](std::complex<double> z){return z.real() / coef_sum;});

    // std::cout << eigen_vec << std::endl;
    // for (int i = 0; i < )
    // *(eigensolver.eigenvectors().colwise().begin());

    return 0;
}

void cfg_character::get_embed_space()
{
    Eigen::VectorXd Phi_inverse_diag = Eigen::VectorXd::Zero(bb_amount);
    std::transform(transition_m_eigen_vec.begin(), transition_m_eigen_vec.end(), Phi_inverse_diag.begin(), [](double x){ if (x) return 1 / x; return 0.0;});
    Eigen::MatrixXd Phi_matrix = transition_m_eigen_vec.asDiagonal();
    Eigen::MatrixXd Phi_inverse = Phi_inverse_diag.asDiagonal();

    // std::cout << Phi_matrix << std::endl;
    // std::cout << Phi_inverse << std::endl;

    Eigen::MatrixXd Phi_inverse_Laplacian = Eigen::MatrixXd::Identity(bb_amount, bb_amount) - 0.5 * (transition_matrix +
    Phi_inverse * transition_matrix.transpose() * Phi_matrix);

    Eigen::EigenSolver<Eigen::MatrixXd> eigensolver(Phi_inverse_Laplacian);
    if (eigensolver.info() != Eigen::Success)
        abort();

    // std::cout << eigensolver.eigenvectors() << std::endl;

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

    smallest_eigen_vec = Eigen::MatrixXd::Zero(bb_amount, embed_vec_count);

    for (auto&& row_it = smallest_eigen_vec.colwise().begin(); row_it != smallest_eigen_vec.colwise().end();)
    {
        double small_eigen = smallest_eigen.top();
        for (auto&& index : eigen_to_indexes_map[small_eigen])
        {
            Eigen::VectorXcd&& to_project = *(eigensolver.eigenvectors().colwise().begin() + index);
            std::transform(to_project.begin(), to_project.end(), row_it->begin(), [](std::complex<double> z){return z.real();});
            ++row_it;
        }
        smallest_eigen.pop();
    }

    // std::cout << smallest_eigen_vec << std::endl;
    // std::cout << bb_amount << std::endl;
    // std::cout << smallest_eigen_vec.rows() << ' ' << smallest_eigen_vec.cols() << std::endl;

    // for (int i = 0; i < bb_amount * emebd_vec_cout; i++)
    //     std::cout << *(smallest_eigen_vec.data() + i) << std::endl;

    // std::cout << "The eigenvalues of Phi_inverse_Laplacian are:\n" << eigensolver.eigenvalues() << std::endl;
    // std::cout << "Here's a matrix whose columns are eigenvectors of A \n"
    //     << "corresponding to these eigenvalues:\n"
    //     << eigensolver.eigenvectors() << std::endl;

    // std::cout << "Saved:" << std::endl;
    // for (auto&& it : eigen_vec)
    //     std::cout << it << ' ';
}

void cfg_character::compress_with_tSNE()
{
    static constexpr int ndim = 1;
    auto Y = qdtsne::initialize_random<ndim>(bb_amount); // initial coordinates

    qdtsne::Tsne<ndim> tSNEcompressor;
    if (smallest_eigen_vec.cols() <= 3)
        return;

    if (3 * qdtsne::Tsne<2, double>::Defaults::perplexity >= smallest_eigen_vec.cols())
    {
        // std::cout << smallest_eigen_vec.cols() / 3 << std::endl;
        if (smallest_eigen_vec.cols() < 6)
            tSNEcompressor.set_perplexity(1);
        else
            tSNEcompressor.set_perplexity(smallest_eigen_vec.cols() / 3 - 1);
    }
    auto ref = tSNEcompressor.run(smallest_eigen_vec.data(), smallest_eigen_vec.rows(), smallest_eigen_vec.cols(), Y.data());
    std::copy(Y.data(), Y.data() + one_bb_character_size, cfg_embedding.begin());
}