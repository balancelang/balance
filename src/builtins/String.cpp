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

    // Define the string type as a { i32*, i32 } - pointer to the string and size of the string
    currentPackage->currentModule->addType(bclass);
    BalanceType * i8pType = new BalanceType(currentPackage->currentModule, "charPointer", llvm::Type::getInt8PtrTy(*currentPackage->context));
    i8pType->isSimpleType = true;
    bclass->properties["stringPointer"] = new BalanceProperty("stringPointer", i8pType, 0, false);

    BalanceType * i32Type = new BalanceType(currentPackage->currentModule, "int", llvm::Type::getInt32Ty(*currentPackage->context));
    i32Type->isSimpleType = true;
    bclass->properties["length"] = new BalanceProperty("length", i32Type, 1, true);

    currentPackage->currentModule->currentType = bclass;
    StructType *structType = StructType::create(*currentPackage->context, "String");
    ArrayRef<Type *> propertyTypesRef({
        // Pointer to the String
        i8pType->getInternalType(),
        // Size of the string
        i32Type->getInternalType()
        // TODO: We could have an optional pointer to the next part of the string
    });
    structType->setBody(propertyTypesRef, false);
    bclass->internalType = structType;
    bclass->hasBody = true;

    createDefaultConstructor(currentPackage->currentModule, bclass);

    createMethod_String_toString();

    currentPackage->currentModule->currentType = nullptr;
}
