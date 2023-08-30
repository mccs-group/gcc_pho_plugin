#ifndef AUTOPHASE_PLUGIN_HH
#define AUTOPHASE_PLUGIN_HH

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <array>
#include <chrono>

#include "cfg_character.hh"
#include "gimple_character.hh"
#include "rtl_character.hh"
#include "gimple_val_flow.hh"

#define IF_MEASURE_TIME 0

const pass_data gimple_character_data =
{
    GIMPLE_PASS,
    "gimple_character",
    OPTGROUP_NONE,
    TV_NONE,
    PROP_gimple_any,
    0,
    0,
    0,
    0
};


const pass_data rtl_character_data =
{
    RTL_PASS,
    "rtl_character",
    OPTGROUP_NONE,
    TV_PLUGIN_INIT,
    0,
    0,
    0,
    0,
    0,
};

class gimple_character_pass: public gimple_opt_pass
{
    gimple_character characteriser;
    cfg_character cfg_char;
    val_flow_character val_char;
    long int whole_time{0};
    enum characterisations
    {
        NONE,
        AUTOPHASE_LIKE,
        CFG,
        VAL_FLOW,
    };

    characterisations cur = AUTOPHASE_LIKE;


public:
    gimple_character_pass(gcc::context* g) : gimple_opt_pass(gimple_character_data, g)
    {}
    opt_pass* clone() override {return this;}

    virtual unsigned int execute (function* fun) override
    {
        // std::cout << "=========" << function_name(fun) << "========="  << std::endl;
        // std::cout << "=========" << IDENTIFIER_POINTER(decl_assembler_name(current_function_decl)) << "=========" << std::endl;
        #if IF_MEASURE_TIME
        auto start = std::chrono::high_resolution_clock::now();
        #endif
        switch(cur)
        {
            case NONE:
                break;
            case AUTOPHASE_LIKE:
                characteriser.parse_function(fun);
                // for (int i = 0; i < characteriser.CHARACTERISTICS_AMOUNT; i++)
                // {
                //     std::cout << *(characteriser.data() + i) << ", ";
                // }
                // std::cout << std::endl;
                characteriser.reset();
                break;
            case CFG:
                #if IF_MEASURE_TIME
                std::cout << fun->cfg->x_n_basic_blocks << std::endl;
                #endif
                cfg_char.get_adjacency_array(fun);
                // for (int i = 0; i < cfg_char.adjacency_array_size(); i++)
                // {
                //     std::cout << *(cfg_char.adjacency_array_data() + i) << ", ";
                // }
                // std::cout << std::endl;

                break;
            case VAL_FLOW:
                #if IF_MEASURE_TIME
                std::cout << fun->last_stmt_uid << std::endl;
                #endif
                val_char.get_adjacency_array(fun);
                // for (int i = 0; i < val_char.adjacency_array_size(); i++)
                // {
                //     std::cout << *(val_char.adjacency_array_data() + i) << ", ";
                // }
                // std::cout << std::endl;
                break;
            default:
                break;

        }

        #if IF_MEASURE_TIME
        auto delta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
        whole_time += delta;
        std::cout << "on " << function_name(fun) << " took " << delta << std::endl;
        std::cout << "whole time: " << whole_time << std::endl;
        #endif
        return 0;
        // std::cout << "=========" << function_name(fun) << "========="  << std::endl;
    }
};

class rtl_character_pass : public rtl_opt_pass
{
    rtl_character characteriser;

    public:
    rtl_character_pass(gcc::context* g) : rtl_opt_pass(rtl_character_data, g)
    {}
    opt_pass* clone() override {return this;}

    void set_dump(bool flag) {characteriser.to_dump = flag;}

    virtual unsigned int execute (function* fun) override { return characteriser.parse_function(fun); }
};

#endif