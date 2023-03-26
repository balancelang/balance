#include "String.h"
#include "../models/BalanceType.h"
#include "../BalancePackage.h"
#include "../models/BalanceProperty.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createMethod_String_toString() {
    BalanceType *stringType = currentPackage->currentModule->getType("String");

    // Create forward declaration of memcpy
    // void * memcpy ( void * destination, const void * source, size_t num );
    ArrayRef<Type *> memcpyParams({
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::Type::getInt64Ty(*currentPackage->context),
    });
    llvm::Type * memcpyReturnType = llvm::Type::getInt32Ty(*currentPackage->context);
    llvm::FunctionType * memcpyDeclarationType = llvm::FunctionType::get(memcpyReturnType, memcpyParams, false);
    currentPackage->builtinModules["builtins"]->module->getOrInsertFunction("memcpy", memcpyDeclarationType);


    std::string functionName = "toString";
    std::string functionNameWithClass = "String_" + functionName;

    BalanceParameter * valueParameter = new BalanceParameter(stringType, "value");

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        stringType->getReferencableType()
    });

    FunctionType *functionType = FunctionType::get(stringType->getReferencableType(), parametersReference, false);

    llvm::Function * intToStringFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", intToStringFunc);

    BalanceFunction * bfunction = new BalanceFunction(functionName, parameters, stringType);
    currentPackage->currentModule->currentType->addMethod(functionName, bfunction);
    bfunction->function = intToStringFunc;

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = intToStringFunc->arg_begin();
    llvm::Value * stringValue = args++;

    currentPackage->currentModule->builder->CreateRet(stringValue);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createType__String() {
    BalanceType * bclass = new BalanceType(currentPackage->currentModule, "String");

    BalanceType * intType = currentPackage->currentModule->getType("Int");
    bclass->properties["typeId"] = new BalanceProperty("typeId", intType, 0, false);
    currentPackage->currentModule->addType(bclass);

    // Define the string type as a { i32*, i32 } - pointer to the string and size of the string
    BalanceType * charPointerType = new BalanceType(currentPackage->currentModule, "CharPointer", llvm::Type::getInt8PtrTy(*currentPackage->context));
    charPointerType->isSimpleType = true;
    charPointerType->isInternalType = true;
    currentPackage->currentModule->addType(charPointerType);
    bclass->properties["stringPointer"] = new BalanceProperty("stringPointer", charPointerType, 1, false);

    BalanceType * int64Type = currentPackage->currentModule->getType("Int64");
    bclass->properties["length"] = new BalanceProperty("length", int64Type, 2, true);

    currentPackage->currentModule->currentType = bclass;
    StructType *structType = StructType::create(*currentPackage->context, "String");
    ArrayRef<Type *> propertyTypesRef({
        bclass->properties["typeId"]->balanceType->getReferencableType(),
        // Pointer to the String
        charPointerType->getInternalType(),
        // Size of the string
        int64Type->getInternalType()
    });
    structType->setBody(propertyTypesRef, false);
    bclass->internalType = structType;
    bclass->hasBody = true;

    createDefaultConstructor(currentPackage->currentModule, bclass);

    currentPackage->currentModule->currentType = nullptr;
}

void createFunctions__String() {
    currentPackage->currentModule->currentType = currentPackage->currentModule->getType("String");
    createMethod_String_toString();
    currentPackage->currentModule->currentType = nullptr;
}
