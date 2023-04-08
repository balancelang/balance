#ifndef FILE_BALANCE_TYPE_H
#define FILE_BALANCE_TYPE_H

#include "BuiltinType.h"

class FileBalanceType : public BuiltinType {
    void registerType() override;
    void finalizeType() override;

    void registerMethods() override;
    void finalizeMethods() override;

    void registerFunctions() override;
    void finalizeFunctions() override;
};

#endif
