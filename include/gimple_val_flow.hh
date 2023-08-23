#ifndef GIMPLE_VAL_FLOW_HH
#define GIMPLE_VAL_FLOW_HH

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <array>

#include <Eigen/Core>
#include <Eigen/SVD>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

#include "qdtsne/qdtsne.hpp"

#include "gcc-plugin.h"

#include "context.h"
#include "function.h"
#include "is-a.h"
#include "predict.h"
#include "basic-block.h"
#include "tree.h"
#include "tree-ssa-alias.h"
#include "gimple-expr.h"
#include "gimple.h"
#include "gimple-ssa.h"
#include "tree-pass.h"
#include "tree-ssa-operands.h"
#include "tree-phinodes.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "gimple-pretty-print.h"

#include "ssa-iterators.h"

#define VAL_FLOW_DEBUG 0

class val_flow_character
{
    Eigen::MatrixXd def_use_matrix;
    Eigen::MatrixXd proximity_matrix;

    std::vector<double> D_src_embed;
    std::vector<double> D_dst_embed;

    std::vector<double> val_flow_embed;
    std::vector<int> adjacency_arr;

    walk_stmt_info walk_info;

    #if VAL_FLOW_DEBUG
    std::vector<std::string> gimple_stmt_names;
    std::vector<std::string> tree_node_names;
    #endif

public:
    static constexpr double COMPARISON_PRECISION = 1e-6;
    static constexpr int STANDART_D_MAT_OBS_LEN = 7;

    double decay_param = 0.8;
    int stmt_amount = 0;
    int max_path_length = 3;
    int D_matrix_characterisation_len = STANDART_D_MAT_OBS_LEN;

    int embed_vec_len = 0;
    int D_mat_rows = 0;

    bool get_full_embed = true;

    gimple* current_gs = nullptr;
    tree current_load_node = nullptr;

private:

    void get_stmt_def_use(gimple* gs);
    void get_all_ssa_uses(tree ssa_name, gimple* gs);
    void print_ssa_info(tree var);
    void get_val_flow_matrix(function* fun);
    void get_proximity_matrix();
    void get_embed_matrices();
    void compress(double* data);

    void reset_walk_info();

    void walk_aliased_vdefs(tree node);

public:
    val_flow_character()
    {
        #if VAL_FLOW_DEBUG
        #define DEFGSCODE(SYM, STRING, STRUCT)	gimple_stmt_names.push_back(STRING);
        #include "gimple.def"
        #undef DEFGSCODE

        #define DEFTREECODE(SYM, STRING, TYPE, NARGS) tree_node_names.push_back(STRING);
        #define END_OF_BASE_TREE_CODES LAST_AND_UNUSED_TREE_CODE,
        #include "all-tree.def"
        #undef DEFTREECODE
        #undef END_OF_BASE_TREE_CODES
        #endif

        walk_info.pset = new hash_set<tree>;
        walk_info.info = this;
    }
    ~val_flow_character()
    {
        delete walk_info.pset;
    }

    // returns a pointer to array, where each odd element is def stmt, and folowing it - use stmt
    int* get_adjacency_array(function* fun);

    void parse_function(function* fun);

    double* data(){return val_flow_embed.data();};

    tree handle_stmt(gimple* stmt,  bool* handled_op);
    bool handle_aliased_vdef(ao_ref* ref , tree node);

    void reset();


};

#endif