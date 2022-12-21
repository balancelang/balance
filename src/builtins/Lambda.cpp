#include "Lambda.h"

#include "../Builtins.h"
#include "../BalancePackage.h"

extern BalancePackage *currentPackage;

void createType__Lambda() {
    BalanceClass * bclass = new BalanceClass(new BalanceTypeString("Lambda"), currentPackage->currentModule);
    currentPackage->currentModule->classes["Lambda"] = bclass;

    currentPackage->currentModule->currentClass = bclass;
    StructType *structType = StructType::create(*currentPackage->context, "Lambda");
    ArrayRef<Type *> propertyTypesRef({});
    structType->setBody(propertyTypesRef, false);
    bclass->internalType = structType;
    bclass->hasBody = true;

    createDefaultConstructor(currentPackage->currentModule, bclass);

    currentPackage->currentModule->currentClass = nullptr;
}