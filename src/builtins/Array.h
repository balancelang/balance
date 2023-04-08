#ifndef ARRAY_TYPE_H
#define ARRAY_TYPE_H

#include "BuiltinType.h"

class ArrayBalanceType : public BuiltinType {
public:
    void registerType() override;
    void finalizeType() override;

    void registerMethods() override;
    void finalizeMethods() override;

    void registerFunctions() override;
    void finalizeFunctions() override;

    BalanceType * registerGenericType(BalanceType * generic);
    void registerMethod_toString(BalanceType * arrayType);
    void finalizeMethod_toString(BalanceType * arrayType);
};

#endif
