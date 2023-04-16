#include "None.h"

#include "../BalancePackage.h"

extern BalancePackage *currentPackage;

void NoneBalanceType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "None");
    this->balanceType->isSimpleType = true;
    currentPackage->currentModule->addType(this->balanceType);
}

void NoneBalanceType::finalizeType() {
    this->balanceType->internalType = llvm::Type::getVoidTy(*currentPackage->context);
}

void NoneBalanceType::registerMethods() {

}

void NoneBalanceType::finalizeMethods() {

}

void NoneBalanceType::registerFunctions() {

}

void NoneBalanceType::finalizeFunctions() {

}
