#ifndef BALANCE_INTERFACE_H
#define BALANCE_INTERFACE_H

#include "BalanceType.h"
#include "BalanceProperty.h"
#include "BalanceFunction.h"
#include "BalanceModule.h"

#include "llvm/IR/DerivedTypes.h"


class BalanceModule;
class BalanceFunction;

class BalanceInterface : public BalanceType {
public:
    BalanceModule *module;
    llvm::StructType *structType = nullptr;
    llvm::StructType *vTableStructType = nullptr;

    BalanceInterface(BalanceTypeString * name)
    {
        this->name = name;
    }

    bool finalized();
};

/*
Each interface defines a vtable of its functions.
Each type defines an instance of the interface vtable, with the concrete implemented functions.
Each instance of a type, holds a pointer to each implemented interface vtable.
The index of each function in the interface is known at compile-time, and hence we can use the same index in the pointed to vtable.
*/

#endif