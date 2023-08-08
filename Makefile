#Env variable AARCH_GCC with name of target compiler is required
TARGET_GCC=$(AARCH_GCC)

#Get path to header files from target gcc
INCLUDE_DIR=$(shell $(TARGET_GCC) -print-file-name=plugin)

SRC_DIR=src
SRC_FILES=plugin.cc

CXX=g++
CXX_FLAGS= -I$(INCLUDE_DIR)/include -I./ -fno-rtti -fPIC -O2

plugin.so: $(addprefix $(SRC_DIR)/, $(SRC_FILES)) extern_makers.cc pass_makers.cc
	$(CXX) -shared $(CXX_FLAGS) $(SRC_DIR)/plugin.cc -o $@

makers_gen.elf: $(SRC_DIR)/makers_gen.cc 
	$(CXX) $^ -o $@

pass_makers.cc: pass_makers.conf makers_gen.elf 
	./makers_gen.elf pass_makers.conf
