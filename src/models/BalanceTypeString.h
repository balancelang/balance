#ifndef BALANCE_TYPE_STRING_H
#define BALANCE_TYPE_STRING_H

#include "llvm/IR/Type.h"

using namespace std;

class BalanceTypeString
{
public:
    std::string base;                                   // E.g. "Array" in "Array<Int>"
    std::vector<BalanceTypeString *> generics;          // E.g. ["String", "Int"] in Dictionary<String, Int>

    BalanceTypeString(std::string base, std::vector<BalanceTypeString *> generics = {}) {
        this->base = base;
        this->generics = generics;
    }
};

#endif