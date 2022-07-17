#ifndef BALANCE_PARAMETER_H
#define BALANCE_PARAMETER_H

#include "llvm/IR/Type.h"

class BalanceParameter
{
public:
    std::string typeString;
    std::string name;
    llvm::Type *type = nullptr;

    BalanceParameter(std::string typeString, std::string name)
    {
        this->typeString = typeString;
        this->name = name;
    }

    bool finalized();
};

#endif