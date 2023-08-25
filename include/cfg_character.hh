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
public:
    double perturbation_factor;
    int one_bb_character_size;

    static constexpr double COMPARISON_PRECISION = 1e-6;
    static constexpr double standart_perturbation_factor = 0.99;
    static constexpr int standart_on_bb_char_size = 7;
private:
    Eigen::MatrixXd adjacency;
    Eigen::MatrixXd invert_out_degrees;
    Eigen::MatrixXd transition_matrix;
    Eigen::VectorXd transition_m_eigen_vec;
    Eigen::MatrixXd smallest_eigen_vec;

    std::vector<double> cfg_embedding;

    std::vector<int> adjacency_array;

    int bb_amount = 0;

    bool get_full_embed = true;

    const static int service_bb_amount = 2;

public:
    cfg_character() : perturbation_factor(standart_perturbation_factor), one_bb_character_size(standart_on_bb_char_size),
                      cfg_embedding(one_bb_character_size, 0)
    {}
    int parse_function(function * fun);

    void get_adjacency_array(function * fun);
    int* adjacency_array_data() { return adjacency_array.data(); }
    int adjacency_array_size() { return adjacency_array.size(); }

    void reset();

private:
    void get_adjacency_matrix(function * fun);
    void get_transition_matrix(function * fun);
    int get_transition_eigen_vector();
    void get_embed_space();
    void compress_with_tSNE();
};

#endif