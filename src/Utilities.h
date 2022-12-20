#ifndef UTILITIES_H
#define UTILITIES_H

#include "visitors/Visitor.h"
#include "models/BalanceModule.h"
#include "models/BalanceFunction.h"
#include "models/BalanceClass.h"

#include <string>
#include <iostream>
#include <cstdio>
#include <fstream>

bool fileExist(std::string fileName);

void createImportedFunction(BalanceModule * bmodule, BalanceFunction * bfunction);
void createImportedClass(BalanceModule * bmodule, BalanceClass * bclass);

void createDefaultConstructor(BalanceModule * bmodule, BalanceClass * bclass);
bool balanceTypeStringsEqual(BalanceTypeString * a, BalanceTypeString * b);
#endif