#ifndef UTILITIES_H
#define UTILITIES_H

#include "visitors/Visitor.h"
#include "models/BalanceModule.h"
#include "models/BalanceFunction.h"
#include "models/BalanceType.h"

#include <string>
#include <iostream>
#include <cstdio>
#include <fstream>

bool fileExist(std::string fileName);

void createImportedFunction(BalanceModule * bmodule, BalanceFunction * bfunction);
void createImportedClass(BalanceModule * bmodule, BalanceType * btype);

void createDefaultConstructor(BalanceModule * bmodule, BalanceType * btype);
#endif