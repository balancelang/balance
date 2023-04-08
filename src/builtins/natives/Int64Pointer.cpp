#include "Int64Pointer.h"

void Int64PointerType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Int64Pointer", llvm::Type::getInt64PtrTy(*currentPackage->context));
}

void Int64PointerType::finalizeType() {

}

void Int64PointerType::registerMethods() {

}

void Int64PointerType::finalizeMethods() {

}

void Int64PointerType::registerFunctions() {

}

void Int64PointerType::finalizeFunctions() {

}

