#include "cfg_character.hh"

#include <numeric>
#include <iostream>


void cfg_character::get_cfg_embed(function * fun)
{
    get_adjacency_matrix(fun);
    get_transition_matrix(fun);
    get_eigen_vectors();
    get_embed_space();
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
    for (auto&& row_it = adjacency.rowwise().begin(); row_it != adjacency.rowwise().end(); row_it++, i++)
    {
        if (std::all_of(row_it->begin(), row_it->end(), [](double num){return num == 0;}))
            mu_vec[i] = 1;
    }

    // for (auto&& it : mu_vec)
    //     std::cout << it << ' ';

    double norm_factor = 1.0 / bb_amount;
    transition_matrix.noalias() = perturbation_factor * (invert_out_degrees * adjacency + norm_factor * mu_vec * Eigen::VectorXd::Constant(bb_amount, 1).transpose())
                                + (1 - perturbation_factor) * norm_factor * Eigen::MatrixXd::Constant(bb_amount, bb_amount, 1);

    // std::cout << 'P' << std::endl;
    // std::cout << transition_matrix << std::endl;
}

void cfg_character::get_eigen_vectors()
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


    auto eigen_one_finder = [](std::complex<double> z){ return (std::abs(z.imag()) < 1e-6) && (std::abs(z.real() - 1.0) < 1e-6);};
    auto it = std::find_if(eigensolver.eigenvalues().begin(), eigensolver.eigenvalues().end(), eigen_one_finder);
    int index = std::distance(eigensolver.eigenvalues().begin(), it);

    if (it == eigensolver.eigenvalues().end())
        std::cout << "shit" << std::endl;
    else
    {
        // auto&& val = eigensolver.eigenvalues()[index];
        // std::cout << "index " << index << ' ' << val << std::endl;
        // std::cout << "vec" << std::endl << *(eigensolver.eigenvectors().colwise().begin()) << std::endl;
        // std::cout << "mult with matrix" << std::endl;
        // std::cout << (to_solve * *(eigensolver.eigenvectors().colwise().begin()));
    }

    Eigen::VectorXcd complex_eigen_vec = *(eigensolver.eigenvectors().colwise().begin() + index);
    eigen_vec = Eigen::VectorXd::Zero(bb_amount);
    std::transform(complex_eigen_vec.begin(), complex_eigen_vec.end(), eigen_vec.begin(), [](std::complex<double> z){return z.real();});

    double coef_sum = std::reduce(eigen_vec.begin(), eigen_vec.end());
    std::transform(eigen_vec.begin(), eigen_vec.end(), eigen_vec.begin(), [coef_sum](double x){return x / coef_sum;});

    // std::cout << eigen_vec << std::endl;
    // for (int i = 0; i < )
    // *(eigensolver.eigenvectors().colwise().begin());

}

void cfg_character::get_embed_space()
{
    Eigen::VectorXd Phi_inverse_diag = Eigen::VectorXd::Zero(bb_amount);
    std::transform(eigen_vec.begin(), eigen_vec.end(), Phi_inverse_diag.begin(), [](double x){ if (x) return 1 / x; return 0.0;});
    Eigen::MatrixXd Phi_matrix = eigen_vec.asDiagonal();
    Eigen::MatrixXd Phi_inverse = Phi_inverse_diag.asDiagonal();

    // std::cout << Phi_matrix << std::endl;
    // std::cout << Phi_inverse << std::endl;

    Eigen::MatrixXd Phi_inverse_Laplacian = Eigen::MatrixXd::Identity(bb_amount, bb_amount) - 0.5 * (transition_matrix +
    Phi_inverse * transition_matrix.transpose() * Phi_matrix);

    Eigen::EigenSolver<Eigen::MatrixXd> eigensolver(Phi_inverse_Laplacian);
    if (eigensolver.info() != Eigen::Success)
        abort();

    // std::cout << "The eigenvalues of Phi_inverse_Laplacian are:\n" << eigensolver.eigenvalues() << std::endl;
    // std::cout << "Here's a matrix whose columns are eigenvectors of A \n"
    //     << "corresponding to these eigenvalues:\n"
    //     << eigensolver.eigenvectors() << std::endl;
}