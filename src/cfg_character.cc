#include "cfg_character.hh"

#include <numeric>
#include <iostream>
#include <queue>
#include <unordered_map>

void cfg_character::get_adjacency_array(function * fun)
{
    reset();
    adjacency_array.reserve(fun->cfg->x_n_basic_blocks);
    adjacency_array.push_back(fun->cfg->x_n_basic_blocks - service_bb_amount);

    bb_amount = fun->cfg->x_n_basic_blocks - service_bb_amount; // special empty basic blocks with id 0 and 1 which are entry/exit are not interesting

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
                adjacency_array.push_back(current_id);
                adjacency_array.push_back(suc_index);
        }
    }

    // std::cout << adjacency << std::endl;
    // std::cout << invert_out_degrees << std::endl;
}

void cfg_character::reset()
{
    adjacency_array.clear();
}