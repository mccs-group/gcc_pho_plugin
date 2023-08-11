#ifndef AUTOPHASE_PLUGIN_HH
#define AUTOPHASE_PLUGIN_HH

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <array>

#include "gcc-plugin.h"

#include "gimple_character.hh"
#include "rtl_character.hh"

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

public:
    gimple_character_pass(gcc::context* g) : gimple_opt_pass(gimple_character_data, g)
    {}
    opt_pass* clone() override {return this;}

    virtual unsigned int execute (function* fun) override { characteriser.parse_function(fun); characteriser.reset(); return 0; }
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