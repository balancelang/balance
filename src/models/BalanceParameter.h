#ifndef BALANCE_PARAMETER_H
#define BALANCE_PARAMETER_H

#include "BalanceType.h"

#include "llvm/IR/Type.h"

class BalanceType;

class BalanceParameter
{
public:
    std::string name;
    BalanceType * balanceType;

    BalanceParameter(BalanceType * balanceType, std::string name)
    {
        this->balanceType = balanceType;
        this->name = name;
    }
};

#endif