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


const pass_data rtl_character_data = {
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
        switch(cur)
        {
            case AUTOPHASE_LIKE:
                characteriser.parse_function(fun);
                characteriser.reset();
                break;
            case CFG:
                // auto start = std::chrono::high_resolution_clock::now();

                cfg_char.get_cfg_embed(fun);

                // auto delta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
                // whole_time += delta;
                // std::cout << "on " << get_name(fun->decl) << " took " << delta << std::endl;
                // std::cout << "whole time: " << whole_time << std::endl;
                break;
            case VAL_FLOW:
                val_char.parse_function(fun);
                break;
            case NONE:
            default:
                break;

        }
        // std::cout << "=========" << function_name(fun) << "========="  << std::endl;
        return 0;
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