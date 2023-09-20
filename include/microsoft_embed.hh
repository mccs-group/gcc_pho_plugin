#ifndef MICROSOFT_EMBED
#define MICROSOFT_EMBED

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include "qdtsne/qdtsne.hpp"

#define MICROSOFT_EMBED_DEBUG 0

class microsoft_embed
{
public:
    static constexpr double COMPARISON_PRECISION = 1e-6;
    static constexpr double standart_perturbation_factor = 0.99;
    static constexpr int standart_on_bb_char_size = 7;

    double perturbation_factor = standart_perturbation_factor;
    int one_bb_character_size = standart_on_bb_char_size;
private:
    Eigen::MatrixXd adjacency;
    Eigen::MatrixXd invert_out_degrees;
    Eigen::MatrixXd transition_matrix;
    Eigen::VectorXd transition_m_eigen_vec;
    Eigen::MatrixXd smallest_eigen_vec;

    std::vector<double> cfg_embedding;
    int adjcency_size = 0;

public:

    template <typename iter>
    double* get_embed(iter begin, iter end)
    {
        if ((begin + 1) == end)
            return nullptr;
        reset();

        get_adjacency_matrix(begin, end);
        #if MICROSOFT_EMBED_DEBUG
        std::cout << "worked adj mat" << std::endl;
        std::cout << adjacency << std::endl;
        #endif
        get_transition_matrix();
        #if MICROSOFT_EMBED_DEBUG
        std::cout << "trans adj mat" << std::endl;
        std::cout << transition_matrix << std::endl;
        #endif
        if (get_transition_eigen_vector())
        {
            std::cerr << "Fatal, could not find eigen value = 1 on function" << std::endl;
            return nullptr;
        }
        #if MICROSOFT_EMBED_DEBUG
        std::cout << "All ok" << std::endl;
        #endif
        get_embed_space();
        #if MICROSOFT_EMBED_DEBUG
        std::cout << "Got embed space" << std::endl;
        // std::cout << smallest_eigen_vec << std::endl;
        #endif
        compress_with_tSNE();
        #if MICROSOFT_EMBED_DEBUG
        std::cout << "Compressed" << std::endl;
        #endif
        return cfg_embedding.data();
    }


private:

    template <typename iter>
    void get_adjacency_matrix(iter begin, iter end)
    {
        int bb_amount = *begin;
        ++begin;

        adjcency_size = std::max(bb_amount, one_bb_character_size + 1);

        std::vector out_degrees(adjcency_size, 0);
        invert_out_degrees = Eigen::MatrixXd::Zero(adjcency_size, adjcency_size);

        adjacency = Eigen::MatrixXd::Zero(adjcency_size, adjcency_size);
        for (; begin != end; ++(++begin))
        {
            adjacency(*begin, *(begin + 1)) = 1;
            out_degrees[*begin] += 1;
        }

        for (int i = 0; i < adjcency_size; i++)
        {
            int cur_out_degree = out_degrees[i];
            if (cur_out_degree != 0)
                invert_out_degrees(i, i) = 1.0 / cur_out_degree;
        }

    }

    void get_transition_matrix();
    int get_transition_eigen_vector();
    void get_embed_space();
    void compress_with_tSNE();
    void reset();

};

#endif