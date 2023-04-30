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
void createImportedClass(BalanceModule *bmodule, BalanceType * btype);
BalanceType * createGenericType(BalanceModule * bmodule, BalanceType * base, std::vector<BalanceType *> generics);

void registerInitializer(BalanceType * btype);
void finalizeInitializer(BalanceType * btype);
void createDefaultToStringMethod(BalanceType * btype);
llvm::Constant *geti8StrVal(llvm::Module &M, char const *str, llvm::Twine const &name, bool addNull);
bool canAssignTo(ParserRuleContext * ctx, BalanceType * aType, BalanceType * bType);
bool functionEqualTo(BalanceFunction * bfunction, std::string functionName, std::vector<BalanceType *> parameters);
std::vector<BalanceType *> parametersToTypes(std::vector<BalanceParameter *> parameters);
std::vector<BalanceType *> valuesToTypes(std::vector<BalanceValue *> values);

#endif
