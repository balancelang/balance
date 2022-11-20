#ifndef BALANCE_VALUE_H
#define BALANCE_VALUE_H

#include <map>
#include "llvm/IR/Value.h"

#include "BalanceTypeString.h"

class BalanceValue
{
public:
    BalanceTypeString * type = nullptr;
    llvm::Value * value = nullptr;

    BalanceValue(BalanceTypeString * type, llvm::Value * value) {
        this->type = type;
        this->value = value;
    }
};

#endif