#include "String.h"
#include "../models/BalanceClass.h"
#include "../Package.h"
#include "../models/BalanceProperty.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createMethod_String_toString() {
    // Create forward declaration of memcpy
    // void * memcpy ( void * destination, const void * source, size_t num );
    ArrayRef<Type *> memcpyParams({
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::Type::getInt64Ty(*currentPackage->context),
    });
    llvm::Type * memcpyReturnType = llvm::Type::getInt32Ty(*currentPackage->context);
    llvm::FunctionType * memcpyDeclarationType = llvm::FunctionType::get(memcpyReturnType, memcpyParams, false);
    currentPackage->builtins->module->getOrInsertFunction("memcpy", memcpyDeclarationType);


    std::string functionName = "toString";
    std::string functionNameWithClass = "String_" + functionName;

    BalanceParameter * valueParameter = new BalanceParameter(new BalanceTypeString("String"), "value");
    valueParameter->type = getBuiltinType(new BalanceTypeString("String"));

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        getBuiltinType(new BalanceTypeString("String"))
    });

    FunctionType *functionType = FunctionType::get(getBuiltinType(new BalanceTypeString("String")), parametersReference, false);

    llvm::Function * intToStringFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", intToStringFunc);

    currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, parameters, new BalanceTypeString("String"));
    currentPackage->currentModule->currentClass->methods[functionName]->function = intToStringFunc;
    currentPackage->currentModule->currentClass->methods[functionName]->returnType = getBuiltinType(new BalanceTypeString("String"));

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = intToStringFunc->arg_begin();
    llvm::Value * stringValue = args++;

    BalanceClass * stringClass = currentPackage->builtins->getClassFromStructName("String");
    int pointerIndex = stringClass->properties["stringPointer"]->index;
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, pointerIndex, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringValue, {pointerZeroValue, pointerIndexValue});
    auto existingStringValue = currentPackage->currentModule->builder->CreateLoad(pointerGEP);

    int sizeIndex = stringClass->properties["length"]->index;
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, sizeIndex, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringValue, {sizeZeroValue, sizeIndexValue});
    auto existingSizeValue = currentPackage->currentModule->builder->CreateLoad(sizeGEP);
    Value * sizeIncludingQuotes = currentPackage->currentModule->builder->CreateAdd(existingSizeValue, ConstantInt::get(*currentPackage->context, llvm::APInt(32, 2, true)));
    Value * sizeIncludingQuotesAndNull = currentPackage->currentModule->builder->CreateAdd(sizeIncludingQuotes, ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)));

    // Allocate string content memory
    auto newStringContentPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),
        llvm::Type::getInt8Ty(*currentPackage->context),
        ConstantExpr::getSizeOf(llvm::Type::getInt8Ty(*currentPackage->context)),
        sizeIncludingQuotesAndNull,
        nullptr,
        "");
    currentPackage->currentModule->builder->Insert(newStringContentPointer);

    // Allocate string struct memory
    auto newStringPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringClass->structType,                                // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringClass->structType),       // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(newStringPointer);

    ArrayRef<Value *> argumentsReference{newStringPointer};
    currentPackage->currentModule->builder->CreateCall(stringClass->constructor, argumentsReference);

    auto newStringPointerGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, newStringPointer, {pointerZeroValue, pointerIndexValue});
    auto newStringSizeGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, newStringPointer, {sizeZeroValue, sizeIndexValue});

    // Insert start quote (ASCII 34)
    Value * quoteValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 34, true));
    ConstantInt * zeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * destinationStartQuote = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), newStringContentPointer, {zeroValue});
    currentPackage->builtins->builder->CreateStore(quoteValue, destinationStartQuote);

    // memcpy existing string into new string
    ConstantInt * oneValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true));
    Value * destinationAddress = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), newStringContentPointer, {oneValue});
    ArrayRef<Value *> memcpyArguments {destinationAddress, existingStringValue, existingSizeValue};
    Function * memcpyFunc = currentPackage->builtins->module->getFunction("memcpy");
    currentPackage->builtins->builder->CreateCall(memcpyFunc, memcpyArguments);

    // Insert end quote (ASCII 34)
    Value * endQuoteIndex = currentPackage->currentModule->builder->CreateAdd(existingSizeValue, ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)));
    Value * destinationEndQuote = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), newStringContentPointer, {endQuoteIndex});
    currentPackage->builtins->builder->CreateStore(quoteValue, destinationEndQuote);

    // Insert null terminator
    Value * nullIndex = currentPackage->currentModule->builder->CreateAdd(endQuoteIndex, ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)));
    auto zeroConstant = ConstantInt::get(*currentPackage->context, llvm::APInt(8, 0, true));
    Value * nullTerminatorAddress = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), newStringContentPointer, { nullIndex });
    currentPackage->builtins->builder->CreateStore(zeroConstant, nullTerminatorAddress);

    // Store length / content in new string
    currentPackage->currentModule->builder->CreateStore(sizeIncludingQuotes, newStringSizeGEP);
    currentPackage->currentModule->builder->CreateStore(newStringContentPointer, newStringPointerGEP);

    currentPackage->currentModule->builder->CreateRet(newStringPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createType__String() {
    auto typeString = new BalanceTypeString("String");
    BalanceClass * bclass = new BalanceClass(typeString);

    // Define the string type as a { i32*, i32 } - pointer to the string and size of the string
    currentPackage->currentModule->classes["String"] = bclass;
    bclass->properties["stringPointer"] = new BalanceProperty("stringPointer", "", 0, false);
    bclass->properties["stringPointer"]->type = llvm::Type::getInt8PtrTy(*currentPackage->context);
    bclass->properties["length"] = new BalanceProperty("length", "", 1, true);
    bclass->properties["length"]->type = llvm::Type::getInt32Ty(*currentPackage->context);

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

    createMethod_String_toString();

    currentPackage->currentModule->currentClass = nullptr;
}
