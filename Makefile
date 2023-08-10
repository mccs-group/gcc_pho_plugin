#Env variable AARCH_GCC with name of target compiler is required
TARGET_GCC=$(AARCH_GCC)

#Get path to header files from target gcc
GCC_INCLUDE_DIR=$(shell $(TARGET_GCC) -print-file-name=plugin)

SRC_DIR=src
INCL_DIR=include
OBJ_FILES=plugin.o callbacks.o plugin_passes.o pass_makers.o

BUILD_DIR=build

CXX=g++
CXX_FLAGS= -I$(GCC_INCLUDE_DIR)/include -I./$(INCL_DIR) -I./ -fno-rtti -fPIC -O2

plugin.so: $(addprefix $(BUILD_DIR)/, $(OBJ_FILES))
	$(CXX) $(CXX_FLAGS) -shared $(addprefix $(BUILD_DIR)/, $(OBJ_FILES)) -o $@

$(addprefix $(BUILD_DIR)/, $(OBJ_FILES)) : $(BUILD_DIR)/%.o : $(SRC_DIR)/%.cc  $(INCL_DIR)/%.h
	$(CXX) $(CXX_FLAGS) -c -shared $< -o $@

makers_gen.elf: $(SRC_DIR)/makers_gen.cc 
	$(CXX) $^ -o $@

$(SRC_DIR)/pass_makers.cc: pass_makers.conf makers_gen.elf 
	./makers_gen.elf pass_makers.conf > $(SRC_DIR)/pass_makers.cc

$(BUILD_DIR)/plugin.o: $(INCL_DIR)/callbacks.h $(INCL_DIR)/plugin_passes.h extern_makers.cc

.PHONY: clean

clean:
	rm *.elf *.so $(SRC_DIR)/pass_makers.cc $(BUILD_DIR)/*
