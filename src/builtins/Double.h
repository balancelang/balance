#ifndef DOUBLE_TYPE_H
#define DOUBLE_TYPE_H

#include "BuiltinType.h"

class DoubleType : public BuiltinType {
    void registerType() override;
    void finalizeType() override;

    void registerMethods() override;
    void finalizeMethods() override;

    void registerFunctions() override;
    void finalizeFunctions() override;

    void finalizeMethod_toString();
    void registerMethod_toString();
};

#endif
