#ifndef BOOL_TYPE_H
#define BOOL_TYPE_H

#include "BuiltinType.h"

class BoolType : public BuiltinType {
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
