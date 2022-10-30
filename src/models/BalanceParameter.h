#ifndef BALANCE_PARAMETER_H
#define BALANCE_PARAMETER_H

#include "BalanceTypeString.h"

#include "llvm/IR/Type.h"

class BalanceParameter
{
public:
    BalanceTypeString * balanceTypeString;
    std::string name;
    llvm::Type *type = nullptr;  // TODO: Ditch this and use balanceTypeString->type

    BalanceParameter(BalanceTypeString * balanceTypeString, std::string name)
    {
        this->balanceTypeString = balanceTypeString;
        this->name = name;
    }

    bool finalized();
};

#endif