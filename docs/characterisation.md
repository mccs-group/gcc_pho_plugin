# GCC characterisation

Several classes are provided to get characterisation of Gimple/RTL of GCC

## Autophase characterisation 
Classes gimple_character and rtl_character provide autophase-like ([original](https://arxiv.org/pdf/2003.00671.pdf)).

In case of gimple autophase-like characterisation the folowing features are not evaluated:
- Number of 32/64 bit integer constants (no way was found in gimple to find out whether constant is 32 or 64 bit)
- Number of alloca instructions (this LLVM instruction does not have equivalent in Gimple)
- Number of Bitcast instructions (this LLVM instruction does not have equivalent in Gimple)
- Number of GetElementPtr instructions (this LLVM instruction does not have equivalent in Gimple)
- Number of SExt/ZExt instructions (these LLVM instructions do not have equivalent in Gimple)
- Number of Select instructions (this LLVM instruction does not have equivalent in Gimple)
- Number of non-external functions instructions (optimization are made on function level, not on module level, so no need in this feature)
- Number of Br instructions (this LLVM instruction does not have equivalent in Gimple)

In case of RTL autophase-like characterisation the folowing features are not evaluated:
- Everything about phi nodes (no phi nodes are present in RTL)
- Number of calls that return int
- Number of alloca instructions (this LLVM instruction does not have equivalent in RTL)
- Number of Bitcast instructions (this LLVM instruction does not have equivalent in RTL)
- Number of GetElementPtr instructions (this LLVM instruction does not have equivalent in RTL)
- Number of Br instructions (this LLVM instruction does not have equivalent in RTL)
- Number of non-external functions instructions (optimization are made on function level, not on module level, so no need in this feature)

## CFG Characterisation
cfg_character class provides control flow graph characterisation on function level of Gimple/RTL (can be applied to both).
This class forms an adjacency matrix, where a\_ij is 1 if the control might flow from basic block with id i to basic block with id j. It does so by walking all the basic blocks in functions, in which all the outgoing edges are visited and the resulting info about control flow is stored

The adjacency matrix is stored in form of coordinate list (array of (row, column), the value is always 1, so it is not stored, but implied)

Has deprecated functionality to compute embedding from this characterisation, the script in [Compute_embed_script](https://arxiv.org/pdf/2003.00671.pdf) should be used

## Value flow characterisation
gimple_val_flow gives Gimple value flow characterisation.

It creates an adjacency matrix, where a\_ij if statement with id i defines a value, which statement with id j uses. This is done the folowing way: each gimple statement in function is examined whether it defines a ssa value(via macro FOR_EACH_SSA_TREE_OPERAND with flag SSA_OP_DEF), and if so, all statements that use this ssa name are walked (via macro FOR_EACH_IMM_USE_STMT) and corresponding a\_ij is set to one.

It is stored the same way cfg characterisation is stored

Has deprecated functionality to compute embedding from this characterisation, the script in [Compute_embed_script](https://github.com/Aleksey2509/Embed_compute_script.git) should be used