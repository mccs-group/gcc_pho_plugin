#ifndef CFG_CHARACTER_HH
#define CFG_CHARACTER_HH

#include <vector>

#include "gcc-plugin.h"

#include "context.h"
#include "function.h"
#include "basic-block.h"

class cfg_character
{
private:
    std::vector<int> adjacency_array;
    int bb_amount = 0;

    const static int service_bb_amount = 2;

public:
    void get_adjacency_array(function * fun);
    int* adjacency_array_data() { return adjacency_array.data(); }
    int adjacency_array_size() { return adjacency_array.size(); }

    void reset();
};

#endif