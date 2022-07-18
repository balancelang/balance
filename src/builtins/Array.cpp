#include "Array.h"
#include "../Builtins.h"
#include "../Package.h"

#include "../models/BalanceTypeString.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;
extern llvm::Value *accessedValue;
extern std::map<llvm::Value *, BalanceTypeString *> typeLookup;

void createMethod_Array_toString(BalanceClass * arrayClass) {
    BalanceParameter * valueParameter = new BalanceParameter(arrayClass->name, "value");
    valueParameter->type = arrayClass->structType->getPointerTo();

    std::string functionName = "toString";
    std::string functionNameWithClass = arrayClass->name->toString() + "_" + functionName;
    BalanceClass * stringClass = currentPackage->builtins->getClassFromBaseName("String");

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        arrayClass->structType->getPointerTo()
    });

    FunctionType *functionType = FunctionType::get(stringClass->structType->getPointerTo(), parametersReference, false);
    llvm::Function * arrayToStringFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->builtins->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", arrayToStringFunc);

    arrayClass->methods[functionName] = new BalanceFunction(functionName, parameters, new BalanceTypeString("String"));
    arrayClass->methods[functionName]->function = arrayToStringFunc;
    arrayClass->methods[functionName]->returnType = stringClass->structType->getPointerTo();

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->builtins->builder->GetInsertBlock();
    currentPackage->builtins->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = arrayToStringFunc->arg_begin();
    llvm::Value * arrayValue = args++;

    // Get length (N)
    auto zeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto lengthIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayClass->properties["length"]->index, true));
    auto pointerGEP = currentPackage->builtins->builder->CreateGEP(arrayClass->structType, arrayValue, {zeroValue, lengthIndexValue});
    Value * lengthValue = currentPackage->builtins->builder->CreateLoad(pointerGEP);

    auto elementSize = ConstantExpr::getSizeOf(stringClass->structType->getPointerTo());
    auto allocize = currentPackage->builtins->builder->CreateMul(elementSize, lengthValue);

    // Allocate N string pointers
    auto arrayMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->builtins->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringClass->structType->getPointerTo(),                // output type, which we get pointer to?
        ConstantExpr::getSizeOf(arrayClass->structType),        // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->builtins->builder->Insert(arrayMemoryPointer);

    // TODO: Create a map function that works on arrays, where toString can be the mapping

    // Create count variable
    Value * countVariable = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * countVariablePtr = currentPackage->builtins->builder->CreateAlloca(Type::getInt32Ty(*currentPackage->context));
    currentPackage->builtins->builder->CreateStore(countVariable, countVariablePtr);

    // Set up branches similar to while loop, where condition is that count variable < lengthValue

    // BEFORE COPY
    // Set up the 3 blocks we need: condition, loop-block and merge
    Function *function = currentPackage->builtins->builder->GetInsertBlock()->getParent();
    BasicBlock *condBlock = BasicBlock::Create(*currentPackage->context, "loopcond", function);
    BasicBlock *loopBlock = BasicBlock::Create(*currentPackage->context, "loop", function);
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->context, "afterloop", function);

    // Jump to condition
    currentPackage->builtins->builder->CreateBr(condBlock);
    currentPackage->builtins->builder->SetInsertPoint(condBlock);

    // while condition
    Value * existingValueCondition = currentPackage->builtins->builder->CreateLoad(countVariablePtr);
    Value * expression = currentPackage->builtins->builder->CreateICmpSLT(existingValueCondition, lengthValue);

    // Create the condition - if expression is true, jump to loop block, else jump to after loop block
    currentPackage->builtins->builder->CreateCondBr(expression, loopBlock, mergeBlock);

    // Set insert point to the loop-block so we can populate it
    currentPackage->builtins->builder->SetInsertPoint(loopBlock);

    // TODO: load the generic type
    // TODO: get the correct toString-function for the generic type
    // TODO: call toString

    // increment countvariable
    Value * existingValue = currentPackage->builtins->builder->CreateLoad(countVariablePtr);
    Value * incrementedValue = currentPackage->currentModule->builder->CreateAdd(countVariable, ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)));
    currentPackage->builtins->builder->CreateStore(incrementedValue, countVariablePtr);

    // At the end of while-block, jump back to the condition which may jump to mergeBlock or reiterate
    currentPackage->currentModule->builder->CreateBr(condBlock);

    // Make sure new code is added to "block" after while statement
    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    // AFTER COPY


    // TODO: Should we sum the string-lenghts while calling toString on each element?
    // Then we can allocate the required characters (calculate including whitespace, comma etc)







    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->builtins->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringClass->structType,                                // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringClass->structType),       // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->builtins->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> argumentsReference{stringMemoryPointer};
    currentPackage->builtins->builder->CreateCall(stringClass->constructor, argumentsReference);
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["stringPointer"]->index, true));
    auto pointerGEP = currentPackage->builtins->builder->CreateGEP(stringClass->structType, stringMemoryPointer, {pointerZeroValue, pointerIndexValue});
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["length"]->index, true));
    auto sizeGEP = currentPackage->builtins->builder->CreateGEP(stringClass->structType, stringMemoryPointer, {sizeZeroValue, sizeIndexValue});
    // TODO: Store
    currentPackage->builtins->builder->CreateRet(stringMemoryPointer);
    currentPackage->builtins->builder->SetInsertPoint(resumeBlock);
}

void createType__Array(BalanceTypeString * typeString) {
    BalanceClass * arrayClass = new BalanceClass(typeString);
    currentPackage->builtins->classes[typeString->toString()] = arrayClass;

    arrayClass->properties["memoryPointer"] = new BalanceProperty("memoryPointer", "", 0);
    arrayClass->properties["memoryPointer"]->type = typeString->generics[0]->type->getPointerTo();
    arrayClass->properties["length"] = new BalanceProperty("length", "", 1, true);
    arrayClass->properties["length"]->type = llvm::Type::getInt32Ty(*currentPackage->context);

    StructType *structType = StructType::create(*currentPackage->context, typeString->toString());
    ArrayRef<Type *> propertyTypesRef({
        arrayClass->properties["memoryPointer"]->type,
        arrayClass->properties["length"]->type,
    });
    structType->setBody(propertyTypesRef, false);
    arrayClass->structType = structType;
    arrayClass->hasBody = true;

    // TODO: this doesn't seem like a good way to do it
    typeString->type = arrayClass->structType->getPointerTo();

    createDefaultConstructor(currentPackage->builtins, arrayClass);

    createMethod_Array_toString(arrayClass);
}