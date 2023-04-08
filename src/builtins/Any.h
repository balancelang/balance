#ifndef ANY_TYPE_H
#define ANY_TYPE_H

#include "BuiltinType.h"

class AnyType : public BuiltinType {
    void registerType() override;
    void finalizeType() override;

    void registerMethods() override;
    void finalizeMethods() override;

    void registerFunctions() override;
    void finalizeFunctions() override;
};

#endif
