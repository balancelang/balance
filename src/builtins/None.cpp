#include "None.h"

#include "../BalancePackage.h"

extern BalancePackage *currentPackage;

void NoneBalanceType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "None", llvm::Type::getVoidTy(*currentPackage->context));
    this->balanceType->isSimpleType = true;
    currentPackage->currentModule->addType(this->balanceType);
}

void NoneBalanceType::finalizeType() {

}

void NoneBalanceType::registerMethods() {

}

void NoneBalanceType::finalizeMethods() {

}

void NoneBalanceType::registerFunctions() {

}

void NoneBalanceType::finalizeFunctions() {

}
