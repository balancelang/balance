#include "Type.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "../models/BalanceType.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createMethod_Type_toString() {

}

// void createType__TypeInfo() {
//     BalanceType * bclass = new BalanceType(currentPackage->currentModule, "TypeInfo");

//     currentPackage->currentModule->addType(bclass);
//     BalanceType * intType = currentPackage->currentModule->getType("Int");

//     bclass->properties["typeId"] = new BalanceProperty("typeId", intType, 0, false);

//     BalanceType * stringType = new BalanceType(currentPackage->currentModule, "constantStringType", currentPackage->currentModule->builder->CreateGlobalStringPtr("")->getType());
//     bclass->properties["name"] = new BalanceProperty("name", stringType, 1);

//     currentPackage->currentModule->currentType = bclass;
//     StructType *structType = StructType::create(*currentPackage->context, "TypeInfo");
//     ArrayRef<Type *> propertyTypesRef({
//         bclass->properties["typeId"]->balanceType->getReferencableType(),
//         bclass->properties["name"]->balanceType->getReferencableType(),
//     });
//     structType->setBody(propertyTypesRef, false);
//     bclass->internalType = structType;
//     bclass->hasBody = true;

//     createDefaultConstructor(currentPackage->currentModule, bclass);

//     createMethod_Type_toString();

//     currentPackage->currentModule->currentType = nullptr;
// }

BalanceType * createType__Type() {
    // createType__TypeInfo();

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

    createDefaultConstructor(currentPackage->currentModule, bclass);

    createMethod_Type_toString();

    currentPackage->currentModule->currentType = nullptr;

    return bclass;
}