#ifndef BALANCE_INTERFACE_H
#define BALANCE_INTERFACE_H

#include "BalanceProperty.h"
#include "BalanceFunction.h"
#include "BalanceModule.h"

#include "llvm/IR/DerivedTypes.h"


class BalanceModule;
class BalanceFunction;


class BalanceInterface
{
public:
    BalanceTypeString * name;
    map<string, BalanceFunction *> methods = {};
    BalanceModule *module;
    llvm::StructType *structType = nullptr;
    BalanceInterface(BalanceTypeString * name)
    {
        this->name = name;
    }
};

#endif