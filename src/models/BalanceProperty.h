#ifndef BALANCE_PROPERTY_H
#define BALANCE_PROPERTY_H

#include "llvm/IR/Type.h"
#include "BalanceTypeString.h"

using namespace std;

class BalanceProperty
{
public:
    std::string name;
    // TODO: Can this store BalanceType instead?
    BalanceTypeString * stringType;
    int index;
    // public: Whether the property can be accessed directly from Balance
    bool isPublic;
    llvm::Type *type;
    BalanceProperty(std::string name, BalanceTypeString * stringType, int index, bool isPublic = true)
    {
        this->name = name;
        this->stringType = stringType;
        this->index = index;
        this->type = nullptr;
        this->isPublic = isPublic;
    }

    bool finalized();
};

#endif