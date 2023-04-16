#ifndef INT_64_POINTER_TYPE_H
#define INT_64_POINTER_TYPE_H

#include "../BuiltinType.h"

class Int64PointerType : public BuiltinType {
    void registerType() override;
    void finalizeType() override;

    void registerMethods() override;
    void finalizeMethods() override;

    void registerFunctions() override;
    void finalizeFunctions() override;

    void registerMethod_Int_toString();
    void finalizeMethod_Int_toString();
};

#endif
