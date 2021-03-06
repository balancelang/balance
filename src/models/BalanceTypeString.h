#ifndef BALANCE_TYPE_STRING_H
#define BALANCE_TYPE_STRING_H

#include "llvm/IR/Type.h"

using namespace std;

/**
 * TODO: How we do handle types from different modules?
 * E.g. Array<MyClass> where MyClass exists in multiple modules?
 */
class BalanceTypeString
{
public:
    std::string base;                                   // E.g. "Array" in "Array<Int>"
    llvm::Type * type = nullptr;
    std::vector<BalanceTypeString *> generics;          // E.g. ["String", "Int"] in Dictionary<String, Int>

    BalanceTypeString(std::string base, std::vector<BalanceTypeString *> generics = {}) {
        this->base = base;
        this->generics = generics;
    }

    std::string toString();
    bool finalized();
    void populateTypes();
};

#endif

/**
 * Dictionary<String, Array<Array<Array<Int>>>>
 */