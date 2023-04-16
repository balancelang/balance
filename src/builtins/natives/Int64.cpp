#include "Int64.h"

#include "../../BalancePackage.h"
#include "../../models/BalanceType.h"

extern BalancePackage *currentPackage;

void Int64Type::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Int64");
    this->balanceType->isSimpleType = true;
    currentPackage->currentModule->addType(this->balanceType);
}

void Int64Type::finalizeType() {
    this->balanceType->internalType = llvm::Type::getInt64Ty(*currentPackage->context);
}

void Int64Type::registerMethods() {

}

void Int64Type::finalizeMethods() {

}

void Int64Type::registerFunctions() {

}

void Int64Type::finalizeFunctions() {

}
