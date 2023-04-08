#ifndef STRING_TYPE_H
#define STRING_TYPE_H

#include "BuiltinType.h"

class StringType : public BuiltinType {
    void registerType() override;
    void finalizeType() override;

    void registerMethods() override;
    void finalizeMethods() override;

    void registerFunctions() override;
    void finalizeFunctions() override;

    void registerMethod_String_toString();
    void finalizeMethod_String_toString();
};

#endif
