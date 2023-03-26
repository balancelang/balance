#include "Type.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "../models/BalanceType.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

BalanceType * createType__Type() {
    BalanceType * bclass = new BalanceType(currentPackage->currentModule, "Type");

    currentPackage->currentModule->addType(bclass);
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    BalanceType * intType = currentPackage->currentModule->getType("Int");
    bclass->properties["typeId"] = new BalanceProperty("typeId", intType, 0, false);
    bclass->properties["name"] = new BalanceProperty("name", stringType, 1);

    currentPackage->currentModule->currentType = bclass;
    StructType *structType = StructType::create(*currentPackage->context, "Type");
    ArrayRef<Type *> propertyTypesRef({
        bclass->properties["typeId"]->balanceType->getReferencableType(),
        bclass->properties["name"]->balanceType->getReferencableType(),
    });
    structType->setBody(propertyTypesRef, false);
    bclass->internalType = structType;
    bclass->hasBody = true;

    currentPackage->currentModule->currentType = nullptr;

    return bclass;
}

void createFunctions__Type() {
    currentPackage->currentModule->currentType = currentPackage->currentModule->getType("Type");

    createDefaultConstructor(currentPackage->currentModule, currentPackage->currentModule->currentType);
    createDefaultToStringMethod(currentPackage->currentModule->currentType);

    currentPackage->currentModule->currentType = nullptr;
}