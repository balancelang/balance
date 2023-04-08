#ifndef INT_TYPE_H
#define INT_TYPE_H

#include "BuiltinType.h"

class IntType : public BuiltinType {
    void registerType() override;
    void finalizeType() override;

    void registerMethods() override;
    void finalizeMethods() override;

    void registerFunctions() override;
    void finalizeFunctions() override;

    void registerMethod_toString();
    void finalizeMethod_toString();
};

#endif
