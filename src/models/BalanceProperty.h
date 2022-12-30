#ifndef BALANCE_PROPERTY_H
#define BALANCE_PROPERTY_H

#include "BalanceType.h"

using namespace std;

class BalanceType;

class BalanceProperty
{
public:
    std::string name;
    BalanceType * balanceType;
    // Index is populated in FinalizePropertiesVisitor when hierarchy is known
    int index = 0;
    // public: Whether the property can be accessed directly from Balance
    bool isPublic;

    BalanceProperty(std::string name, BalanceType * balanceType, bool isPublic = true)
    {
        this->name = name;
        this->balanceType = balanceType;
        this->isPublic = isPublic;
    }

    BalanceProperty(std::string name, BalanceType * balanceType, int index, bool isPublic = true)
    {
        this->name = name;
        this->balanceType = balanceType;
        this->index = index;
        this->isPublic = isPublic;
    }
};

#endif