#Env variable AARCH_GCC with name of target compiler is required
TARGET_GCC=$(AARCH_GCC)

#Get path to header files from target gcc
PLUGIN_INCLUDE_DIR=$(shell $(TARGET_GCC) -print-file-name=plugin)

SRC_DIR=src
SRC_FILES=plugin.cc

INCLUDE_DIR = include

CXX=g++
PLUGIN_COMPILE_FLAGS= -I$(PLUGIN_INCLUDE_DIR)/include -I./ -fno-rtti -fPIC -O2 -I$(INCLUDE_DIR)

plugin.so: $(addprefix $(SRC_DIR)/, $(SRC_FILES)) extern_makers.cc pass_makers.cc
	$(CXX) -shared $(PLUGIN_COMPILE_FLAGS) $(SRC_DIR)/plugin.cc -o $@

makers_gen.elf: $(SRC_DIR)/makers_gen.cc 
	$(CXX) $^ -o $@

pass_makers.cc: pass_makers.conf makers_gen.elf 
	./makers_gen.elf pass_makers.conf

TEST_SRC_DIR = autophase_tests

AUTOP_PLUGIN_SRC_FILES = autophase_plugin.cc gimple_character.cc rtl_character.cc
AUTOP_PLUGIN_SOURCES = $(addprefix $(SRC_DIR)/, $(AUTOP_PLUGIN_SRC_FILES))

CXXFLAGS = -O2 -I$(INCLUDE_DIR)
PLUGINFLAGS = -fplugin=/home/lexotr/Opt_odg/gcc_dyn_list/autophase_plugin.so
PLUGINFLAGS += -fplugin-arg-autophase_plugin-basic_blocks_optimized
#PLUGINFLAGS += -fplugin-arg-autophase_plugin-basic_blocks_dfinish

preproc_gimple: $(AUTOP_PLUGIN_SOURCES) $(INCLUDE_DIR)/autophase_plugin.hh
	$(CXX) $(PLUGIN_COMPILE_FLAGS) -E $(SRC_DIR)/gimple_character.cc > plugin.preproc

autophase_plugin.so : $(AUTOP_PLUGIN_SOURCES) $(INCLUDE_DIR)/autophase_plugin.hh $(INCLUDE_DIR)/gimple_character.hh $(INCLUDE_DIR)/rtl_character.hh
	$(CXX) $(PLUGIN_COMPILE_FLAGS) -shared $(AUTOP_PLUGIN_SOURCES) -o $@

test_bin: autophase_plugin.so $(TEST_SRC_DIR)/fizzbuzz.c
	$(TARGET_GCC) $(CXXFLAGS) $(PLUGINFLAGS) -c $(TEST_SRC_DIR)/fizzbuzz.c -o /dev/null

test_phi: autophase_plugin.so $(TEST_SRC_DIR)/phi.c
	$(TARGET_GCC) $(CXXFLAGS) $(PLUGINFLAGS) -c $(TEST_SRC_DIR)/phi.c -o /dev/null

test_excep: autophase_plugin.so $(TEST_SRC_DIR)/except.cc
	$(TARGET_GCC) $(CXXFLAGS) $(PLUGINFLAGS) -fdump-tree-optimized -c $(TEST_SRC_DIR)/except.cc -o /dev/null

test_bzip2: autophase_plugin.so $(TEST_SRC_DIR)/bzip2d/blocksort.c
	$(TARGET_GCC) $(CXXFLAGS) $(PLUGINFLAGS) -fdump-tree-optimized -fdump-rtl-dfinish -c $(TEST_SRC_DIR)/bzip2d/blocksort.c -I$(TEST_SRC_DIR)/bzip2d -o /dev/null

