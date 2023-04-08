#ifndef LAMBDA_TYPE_H
#define LAMBDA_TYPE_H

#include "BuiltinType.h"

class LambdaBalanceType : public BuiltinType {
public:
    void registerType() override;
    void finalizeType() override;

    void registerMethods() override;
    void finalizeMethods() override;

    void registerFunctions() override;
    void finalizeFunctions() override;

    void registerMethod_toString();
    void finalizeMethod_toString();

    BalanceType * registerGenericType(std::vector<BalanceType *> generics);
};

#endif
