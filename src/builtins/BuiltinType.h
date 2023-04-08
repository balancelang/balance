#ifndef BUILTIN_TYPE_H
#define BUILTIN_TYPE_H

#include "../models/BalanceType.h"

class BuiltinType {
public:
    BalanceType * balanceType = nullptr;

    virtual void registerType() = 0;
    virtual void finalizeType() = 0;

    virtual void registerMethods() = 0;
    virtual void finalizeMethods() = 0;

    virtual void registerFunctions() = 0;
    virtual void finalizeFunctions() = 0;
};

#endif
