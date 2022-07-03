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
    llvm::Type *type;
    BalanceProperty(std::string name, std::string stringType, int index)
    {
        this->name = name;
        this->stringType = stringType;
        this->index = index;
        this->type = nullptr;
    }

    bool finalized();
};

#endif