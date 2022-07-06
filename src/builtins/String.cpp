#include "String.h"
#include "../models/BalanceClass.h"
#include "../Package.h"
#include "../models/BalanceProperty.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createType__String() {
    // Define the string type as a { i32*, i32 } - pointer to the string and size of the string
    BalanceClass * bclass = new BalanceClass("String");
    currentPackage->currentModule->classes["String"] = bclass;
    bclass->properties["stringPointer"] = new BalanceProperty("stringPointer", "", 0);
    bclass->properties["stringPointer"]->type = llvm::Type::getInt8PtrTy(*currentPackage->context);
    bclass->properties["stringSize"] = new BalanceProperty("stringSize", "", 1);
    bclass->properties["stringSize"]->type = llvm::Type::getInt32Ty(*currentPackage->context);

    currentPackage->currentModule->currentClass = bclass;
    StructType *structType = StructType::create(*currentPackage->context, "String");
    ArrayRef<Type *> propertyTypesRef({
        // Pointer to the String
        llvm::Type::getInt8PtrTy(*currentPackage->context),
        // Size of the string
        llvm::Type::getInt32Ty(*currentPackage->context)
        // TODO: We could have an optional pointer to the next part of the string
    });
    structType->setBody(propertyTypesRef, false);
    bclass->structType = structType;
    bclass->hasBody = true;

    createDefaultConstructor(currentPackage->currentModule, bclass);

    currentPackage->currentModule->currentClass = nullptr;
}
