#ifndef BALANCE_PROPERTY_H
#define BALANCE_PROPERTY_H

#include "llvm/IR/Type.h"

using namespace std;

class BalanceProperty
{
public:
    std::string name;
    std::string stringType; // TODO: Make sure we use the same name convention (BalanceParameter calls it typeString)
    int index;
    // public: Whether the property can be accessed directly from Balance
    bool isPublic;
    llvm::Type *type;
    BalanceProperty(std::string name, std::string stringType, int index, bool isPublic = false)
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