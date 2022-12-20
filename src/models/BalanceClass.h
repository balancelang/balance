#ifndef BALANCE_CLASS_H
#define BALANCE_CLASS_H

#include "BalanceType.h"
#include "BalanceProperty.h"
#include "BalanceFunction.h"
#include "BalanceModule.h"
#include "BalanceInterface.h"

#include "llvm/IR/DerivedTypes.h"

class BalanceImportedFunction;
class BalanceModule;
class BalanceFunction;
class BalanceProperty;
class BalanceInterface;


class BalanceClass : public BalanceType
{
public:
    map<string, BalanceInterface *> interfaces = {};
    map<string, llvm::Value *> interfaceVTables = {};
    bool hasBody;
    llvm::StructType *vTableStructType = nullptr;
    int vtableTypeIndex = 0;
    BalanceModule *module;
    BalanceClass(BalanceTypeString * name, BalanceModule * module)
    {
        this->name = name;
        this->module = module;
    }

    bool compareTypeString(BalanceTypeString * other);
    bool finalized();
};

#endif