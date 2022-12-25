#include "Any.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "../models/BalanceType.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createMethod_Any_toString() {

}

BalanceType * createType__Any() {
    BalanceType * anyType = new BalanceType(currentPackage->currentModule, "Any");
    currentPackage->builtins->types[anyType->toString()] = anyType;
    return anyType;
}