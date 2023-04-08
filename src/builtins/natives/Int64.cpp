#include "Int64.h"

#include "../../BalancePackage.h"
#include "../../models/BalanceType.h"

extern BalancePackage *currentPackage;

void Int64Type::registerType() {
    BalanceType * int64Type = new BalanceType(currentPackage->currentModule, "Int64", llvm::Type::getInt64Ty(*currentPackage->context));
    this->balanceType = int64Type;
    int64Type->isSimpleType = true;
    int64Type->isPublic = false;
    currentPackage->currentModule->addType(int64Type);
}

void Int64Type::finalizeType() {

}

void Int64Type::registerMethods() {

}

void Int64Type::finalizeMethods() {

}

void Int64Type::registerFunctions() {

}

void Int64Type::finalizeFunctions() {

}
