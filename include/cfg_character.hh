#ifndef CFG_CHARACTER_HH
#define CFG_CHARACTER_HH

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include "qdtsne/qdtsne.hpp"

#include "gcc-plugin.h"

#include "context.h"
#include "function.h"
#include "basic-block.h"

class cfg_character
{
    Eigen::MatrixXd adjacency;
    Eigen::MatrixXd invert_out_degrees;
    Eigen::MatrixXd transition_matrix;
    Eigen::VectorXd transition_m_eigen_vec;

    int bb_amount = 0;

    const static int service_bb_amount = 2;

public:
    double perturbation_factor = 0.99;
    int one_bb_character_size = 7;
    static constexpr double COMPARISON_PRECISION = 1e-6;

private:
    void get_adjacency_matrix(function * fun);
    void get_transition_matrix(function * fun);
    void get_eigen_vectors();
    void get_embed_space();

public:
    void get_cfg_embed(function * fun);
};

#endif