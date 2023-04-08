#ifndef INT_8_POINTER_TYPE_H
#define INT_8_POINTER_TYPE_H

#include "../BuiltinType.h"

class Int8PointerType : public BuiltinType {
    void registerType() override;
    void finalizeType() override;

    void registerMethods() override;
    void finalizeMethods() override;

    void registerFunctions() override;
    void finalizeFunctions() override;
};

#endif
