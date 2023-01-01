#ifndef UTILITIES_H
#define UTILITIES_H

#include "visitors/Visitor.h"
#include "models/BalanceModule.h"
#include "models/BalanceFunction.h"
#include "models/BalanceType.h"

#include "llvm/IR/Constants.h"

#include <string>
#include <iostream>
#include <cstdio>
#include <fstream>

bool fileExist(std::string fileName);

void createImportedFunction(BalanceModule * bmodule, BalanceFunction * bfunction);
void createImportedClass(BalanceModule * bmodule, BalanceType * btype);

void createDefaultConstructor(BalanceModule * bmodule, BalanceType * btype);
void createDefaultToStringMethod(BalanceType * btype);
llvm::Constant *geti8StrVal(llvm::Module &M, char const *str, llvm::Twine const &name, bool addNull);
#endif