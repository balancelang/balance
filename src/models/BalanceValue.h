#ifndef BALANCE_VALUE_H
#define BALANCE_VALUE_H

#include <map>
#include "llvm/IR/Value.h"

#include "BalanceType.h"

class BalanceType;

class BalanceValue
{
public:
    BalanceType * type = nullptr;
    llvm::Value * value = nullptr;

    BalanceValue(BalanceType * type, llvm::Value * value) {
        this->type = type;
        this->value = value;
    }
};

#endif