#ifndef BUILTINS_H
#define BUILTINS_H

#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"

// Types



// Functions
void create_function_print();

// All
void createFunctions();
void createTypes();
void createBuiltins();

#endif