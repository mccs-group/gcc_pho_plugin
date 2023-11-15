# GCC characterisation

Several classes are provided to get characterisation of Gimple/RTL of GCC

## Autophase characterisation 
Classes gimple\_character and rtl\_character provide autophase-like ([original](https://arxiv.org/pdf/2003.00671.pdf)).

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
cfg\_character class provides control flow graph characterisation on function level of Gimple/RTL (can be applied to both).
This class forms an adjacency matrix, where a\_ij is 1 if the control might flow from basic block with id i to basic block with id j. It does so by walking all the basic blocks in functions, in which all the outgoing edges are visited and the resulting info about control flow is stored

The adjacency matrix is stored in form of coordinate list (array of (row, column), the value is always 1, so it is not stored, but implied)

## Value flow characterisation
gimple\_val\_flow gives Gimple value flow characterisation.

It creates an adjacency matrix, where a\_ij if statement with id i defines a value, which statement with id j uses. This is done the folowing way: each gimple statement in function is examined whether it defines a ssa value(via macro FOR\_EACH\_SSA\_TREE\_OPERAND with flag SSA\_OP\_DEF), and if so, all statements that use this ssa name are walked (via macro FOR\_EACH\_IMM\_USE\_STMT) and corresponding a\_ij is set to one.

Also, the alias analysis is used. In Gimple, obviously, memory accesses are represented. Also, usual variables, whose underlying memory is needed (for example, a pointer to them is passed into another function) are not transformed into ssa values, even in ssa form, and they are always present as VAR\_DECL. All return, assign, and call 
statements are examined, whether they use memory region (as memory under some variable or memory pointed by pointer). Only those three gimple statements are examined, 
because other gimple statements may deal only with ssa values. When encountering such a statement, we start walking all the statements that might change corresponding 
memory region via walk\_aliased\_vdef function from gcc. When a varible, which aliases currently examined one is encountered in left side of the assignment, or is passed 
by non-const pointer into function, that memory region counts as being defined in that instruction, and a def-use relationship is established between those instructions.

It is stored the same way cfg characterisation is stored
