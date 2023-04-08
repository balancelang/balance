#ifndef FAT_POINTER_TYPE_H
#define FAT_POINTER_TYPE_H

#include "BuiltinType.h"

class FatPointerType : public BuiltinType {
    void registerType() override;
    void finalizeType() override;

    void registerMethods() override;
    void finalizeMethods() override;

    void registerFunctions() override;
    void finalizeFunctions() override;
};

#endif
