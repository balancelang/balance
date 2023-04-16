#ifndef BUILTINS_H
#define BUILTINS_H

#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"

#include "builtins/BuiltinType.h"
#include "builtins/natives/Int8Pointer.h"
#include "builtins/natives/Int64.h"
#include "builtins/natives/Int64Pointer.h"
#include "builtins/Int.h"
#include "builtins/Any.h"
#include "builtins/Array.h"
#include "builtins/Bool.h"
#include "builtins/Double.h"
#include "builtins/FatPointer.h"
#include "builtins/File.h"
#include "builtins/Lambda.h"
#include "builtins/None.h"
#include "builtins/String.h"
#include "builtins/Type.h"

void registerNativeTypes();
void finalizeNativeTypes();

void registerBuiltinTypes();
void finalizeBuiltinTypes();

void registerBuiltinMethods();
void finalizeBuiltinMethods();

void registerBuiltinFunctions();
void finalizeBuiltinFunctions();

void registerFunction__print();
void finalizeFunction__print();

void registerFunction__open();
void finalizeFunction__open();

#endif