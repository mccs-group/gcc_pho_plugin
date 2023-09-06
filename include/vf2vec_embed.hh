#ifndef FLOW2VEC_EMBED
#define FLOW2VEC_EMBED

#include <Eigen/Core>
#include <Eigen/SVD>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

#include "qdtsne/qdtsne.hpp"

#include <vector>

#define VAL_FLOW_EMBED_DEBUG 0

class vf2vec_embed
{
    public:
    static constexpr double COMPARISON_PRECISION = 1e-6;
    static constexpr int STANDART_D_MAT_OBS_LEN = 7;

    double decay_param = 0.8;
    int max_path_length = 3;
    int D_matrix_characterisation_len = STANDART_D_MAT_OBS_LEN;

private:
    int def_use_side_size = 0;
    int embed_vec_len = 0;
    int D_mat_rows = 0;

    Eigen::MatrixXd def_use_matrix;
    Eigen::MatrixXd proximity_matrix;

    std::vector<double> D_src_embed;
    std::vector<double> D_dst_embed;

    std::vector<double> val_flow_embed;

    template <typename iter>
    void get_val_flow_matrix(iter begin, iter end)
    {
        int stmt_amount = *begin;
        ++begin;

        def_use_side_size = std::max(stmt_amount, D_matrix_characterisation_len);
        std::cout << def_use_side_size << std::endl;


        def_use_matrix = Eigen::MatrixXd::Zero(def_use_side_size, def_use_side_size);
        for (; begin != end; ++(++begin))
            def_use_matrix(*begin, *(begin + 1)) = 1;
    }


    void get_proximity_matrix();
    void get_embed_matrices();
    void compress(double* data);
    double* data(){return val_flow_embed.data();};

    void reset();

    template <typename it>
    bool no_nan_matrix(it col_begin, it col_end)
    {
        auto&& bad_col = [](typename std::iterator_traits<it>::value_type col){return std::any_of(col.begin(), col.end(), [](double num){return isnan(num);});};
        if (std::find_if(col_begin, col_end, bad_col) == col_end)
            return true;
        return false;
    }

public:

    template <typename iter>
    double* get_embed(iter begin, iter end)
    {
        if ((begin + 1) == end)
            return nullptr;
        reset();
        // std::cout << "started" << std::endl;
        get_val_flow_matrix(begin, end);
        std::cout << "val flow mat:" << std::endl;
        std::cout << def_use_matrix << std::endl;
        get_proximity_matrix();
        std::cout << "proximity mat:" << std::endl;
        std::cout << proximity_matrix << std::endl;
        get_embed_matrices();

        val_flow_embed.reserve(2 * D_matrix_characterisation_len);

        compress(D_src_embed.data());
        compress(D_dst_embed.data());

        #if VAL_FLOW_EMBED_DEBUG
        for (auto&& it : val_flow_embed)
            std::cout << it << " ";
        #endif

        std::cout << "final size " << val_flow_embed.size() << std::endl;

        return val_flow_embed.data();
    }
};

#endif