#include "Int8Pointer.h"

#include "../../BalancePackage.h"
#include "../../models/BalanceType.h"

extern BalancePackage *currentPackage;

void Int8PointerType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Int8Pointer", llvm::Type::getInt8PtrTy(*currentPackage->context));
    this->balanceType->isSimpleType = true;
    currentPackage->currentModule->addType(this->balanceType);
}

void Int8PointerType::finalizeType() {

}

void Int8PointerType::registerMethods() {

}

void Int8PointerType::finalizeMethods() {

}

void Int8PointerType::registerFunctions() {

}

void Int8PointerType::finalizeFunctions() {

}
