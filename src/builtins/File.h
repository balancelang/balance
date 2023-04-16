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


    void registerMethod_close();
    void registerMethod_read();
    void registerMethod_write();
    void finalizeMethod_close();
    void finalizeMethod_read();
    void finalizeMethod_write();
};

#endif
