#Env variable AARCH_GCC with name of target compiler is required
TARGET_GCC=$(AARCH_GCC)

#Get path to header files from target gcc
PLUGIN_INCLUDE_DIR=$(shell $(TARGET_GCC) -print-file-name=plugin)

SRC_DIR=src
INCL_DIR=include
OBJ_FILES=plugin.o callbacks.o plugin_passes.o pass_makers.o

BUILD_DIR=build

INCLUDE_DIR = include

CXX=g++

EXTERNAL_DIR = external

EXTERNAL_INCLUDES = aarand annoy hnswlib kmeans kncolle powerit qdtnse
EXTERNAL = -I$(EXTERNAL_DIR)/eigen-3.4.0 -I$(EXTERNAL_DIR)
EXTERNAL += $(addprefix -I$(EXTERNAL_DIR)/, $(EXTERNAL_INCLUDES))

PLUGIN_COMPILE_FLAGS= -I$(PLUGIN_INCLUDE_DIR)/include -I./ -fno-rtti -fPIC -O3 -I$(INCLUDE_DIR) $(EXTERNAL) --std=c++17 -DNDEBUG

plugins: plugin.so autophase_plugin.so

plugin.so: $(addprefix $(BUILD_DIR)/, $(OBJ_FILES))
	$(CXX) $(PLUGIN_COMPILE_FLAGS) -shared $(addprefix $(BUILD_DIR)/, $(OBJ_FILES)) -o $@

$(addprefix $(BUILD_DIR)/, $(OBJ_FILES)) : $(BUILD_DIR)/%.o : $(SRC_DIR)/%.cc  $(INCL_DIR)/%.h
	$(CXX) $(PLUGIN_COMPILE_FLAGS) -c -shared $< -o $@

makers_gen.elf: $(SRC_DIR)/makers_gen.cc 
	$(CXX) $^ -o $@
  
$(SRC_DIR)/pass_makers.cc: pass_makers.conf makers_gen.elf
	./makers_gen.elf pass_makers.conf > $(SRC_DIR)/pass_makers.cc

$(BUILD_DIR)/plugin.o: $(INCL_DIR)/callbacks.h $(INCL_DIR)/plugin_passes.h extern_makers.cc

.PHONY: clean clean_obj

TEST_SRC_DIR = autophase_tests

AUTOP_PLUGIN_SRC_FILES = autophase_plugin.cc gimple_character.cc rtl_character.cc cfg_character.cc gimple_val_flow.cc
AUTOP_PLUGIN_SOURCES = $(addprefix $(SRC_DIR)/, $(AUTOP_PLUGIN_SRC_FILES))

AUTOP_OBJECTS = $(AUTOP_PLUGIN_SRC_FILES:.cc=.o)
OBJ_DIR = $(BUILD_DIR)
AUTOP_OBJ := $(addprefix $(OBJ_DIR)/, $(AUTOP_OBJECTS))

CXXFLAGS = -O2 -I$(INCLUDE_DIR)
PLUGINFLAGS = -fplugin=/home/lexotr/Opt_odg/gcc_dyn_list/autophase_plugin.so
PLUGINFLAGS += -fplugin-arg-autophase_plugin-basic_blocks_optimized
#PLUGINFLAGS += -fdump-tree-local-pure-const-vops

# PLUGINFLAGS += -fplugin-arg-autophase_plugin-basic_blocks_dfinish
# PLUGINFLAGS += -fdump-rtl-dfinish

preproc_gimple: $(AUTOP_PLUGIN_SOURCES) $(INCLUDE_DIR)/autophase_plugin.hh
	$(CXX) $(PLUGIN_COMPILE_FLAGS) -E $(SRC_DIR)/gimple_val_flow.cc > plugin.preproc

$(OBJ_DIR)/gimple_character.o : $(SRC_DIR)/gimple_character.cc $(INCLUDE_DIR)/gimple_character.hh
	$(CXX) $(PLUGIN_COMPILE_FLAGS) $< -c -o $@

$(OBJ_DIR)/gimple_val_flow.o : $(SRC_DIR)/gimple_val_flow.cc $(INCLUDE_DIR)/gimple_val_flow.hh
	$(CXX) $(PLUGIN_COMPILE_FLAGS) $< -c -o $@

$(OBJ_DIR)/rtl_character.o : $(SRC_DIR)/rtl_character.cc $(INCLUDE_DIR)/rtl_character.hh
	$(CXX) $(PLUGIN_COMPILE_FLAGS) $< -c -o $@

$(OBJ_DIR)/cfg_character.o : $(SRC_DIR)/cfg_character.cc $(INCLUDE_DIR)/cfg_character.hh
	$(CXX) $(PLUGIN_COMPILE_FLAGS) $< -c -o $@

$(OBJ_DIR)/autophase_plugin.o : $(SRC_DIR)/autophase_plugin.cc $(INCLUDE_DIR)/autophase_plugin.hh\
								$(INCLUDE_DIR)/gimple_character.hh $(INCLUDE_DIR)/rtl_character.hh\
								$(INCLUDE_DIR)/cfg_character.hh $(INCLUDE_DIR)/gimple_val_flow.hh
	$(CXX) $(PLUGIN_COMPILE_FLAGS) $< -c -o $@

autophase_plugin.so : $(AUTOP_OBJ)
	$(CXX) $(PLUGIN_COMPILE_FLAGS) -shared $^ -o $@

test_bin: $(TEST_SRC_DIR)/fizzbuzz.c autophase_plugin.so
	$(TARGET_GCC) $(CXXFLAGS) $(PLUGINFLAGS) -c $< -o /dev/null

test_phi: $(TEST_SRC_DIR)/phi.cc autophase_plugin.so
	$(TARGET_GCC) $(CXXFLAGS) $(PLUGINFLAGS) -c $< -o phi.o

test_mem: $(TEST_SRC_DIR)/mem.c autophase_plugin.so
	$(TARGET_GCC) $(CXXFLAGS) $(PLUGINFLAGS) -c $< -o /dev/null

test_excep: $(TEST_SRC_DIR)/except.cc autophase_plugin.so
	$(TARGET_GCC) $(CXXFLAGS) $(PLUGINFLAGS) -c $< -o /dev/null

test_bzip2: autophase_plugin.so
	$(TARGET_GCC) $(CXXFLAGS) $(PLUGINFLAGS) -c $(TEST_SRC_DIR)/bzip2d/bzlib.c -I$(TEST_SRC_DIR)/bzip2d -o /dev/null

test_ternar: $(TEST_SRC_DIR)/ternar.c autophase_plugin.so
	$(TARGET_GCC) $(CXXFLAGS) $(PLUGINFLAGS) -c $< -o /dev/null

clean_obj:
	rm obj/*

clean:
	rm *.elf *.so $(SRC_DIR)/pass_makers.cc $(BUILD_DIR)/*

