#include "Array.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "../models/BalanceType.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void ArrayBalanceType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Array", {});

    // This registers the base type, which can't be instantiated directly.
    currentPackage->builtinModules["builtins"]->genericTypes["Array"] = this->balanceType;
}

void ArrayBalanceType::finalizeType() {

}

void ArrayBalanceType::registerMethods() {

}

void ArrayBalanceType::finalizeMethods() {

}

void ArrayBalanceType::registerFunctions() {

}

void ArrayBalanceType::finalizeFunctions() {

}

BalanceType * ArrayBalanceType::registerGenericType(BalanceType * generic) {
    BalanceType * arrayType = new BalanceType(currentPackage->currentModule, this->balanceType->name, {generic});
    currentPackage->currentModule->addType(arrayType);

    BalanceType * intType = currentPackage->currentModule->getType("Int");
    BalanceType * genericPointerType = new BalanceType(currentPackage->currentModule, "genericPointerType", arrayType->generics[0]->getReferencableType()->getPointerTo());
    genericPointerType->isSimpleType = true;
    arrayType->properties["memoryPointer"] = new BalanceProperty("memoryPointer", genericPointerType, 0, false);
    arrayType->properties["length"] = new BalanceProperty("length", intType, 1);

    StructType *structType = StructType::create(*currentPackage->context, arrayType->toString());
    ArrayRef<Type *> propertyTypesRef({
        arrayType->properties["memoryPointer"]->balanceType->getInternalType(),
        arrayType->properties["length"]->balanceType->getInternalType(),
    });
    structType->setBody(propertyTypesRef, false);
    arrayType->internalType = structType;
    arrayType->hasBody = true;

    registerInitializer(arrayType);
    finalizeInitializer(arrayType);

    this->registerMethod_toString(arrayType);
    this->finalizeMethod_toString(arrayType);

    return arrayType;
}

void ArrayBalanceType::registerMethod_toString(BalanceType * arrayType) {
    std::string functionName = "toString";
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    BalanceParameter * valueParameter = new BalanceParameter(arrayType, "value");
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };
    BalanceFunction * bfunction = new BalanceFunction(currentPackage->currentModule, arrayType, functionName, parameters, stringType);
    arrayType->addMethod(functionName, bfunction);
}

void ArrayBalanceType::finalizeMethod_toString(BalanceType * arrayType) {
    BalanceFunction * toStringFunction = arrayType->getMethod("toString");
    BalanceType * stringType = currentPackage->currentModule->getType("String");

    // Create forward declaration of memcpy
    // void * memcpy ( void * destination, const void * source, size_t num );
    ArrayRef<Type *> memcpyParams({
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::Type::getInt64Ty(*currentPackage->context),
    });
    llvm::Type * memcpyReturnType = llvm::Type::getInt32Ty(*currentPackage->context);
    llvm::FunctionType * memcpyDeclarationType = llvm::FunctionType::get(memcpyReturnType, memcpyParams, false);
    currentPackage->currentModule->module->getOrInsertFunction("memcpy", memcpyDeclarationType);

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        arrayType->getReferencableType()
    });

    llvm::Function * arrayToStringFunc = Function::Create(toStringFunction->getLlvmFunctionType(), Function::ExternalLinkage, toStringFunction->getFullyQualifiedFunctionName(), currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, toStringFunction->getFunctionName() + "_body", arrayToStringFunc);

    toStringFunction->setLlvmFunction(arrayToStringFunc);

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = arrayToStringFunc->arg_begin();
    Value * arrayValue = args++;

    // Get length (N)
    ConstantInt * zeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    ConstantInt * lengthIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayType->properties["length"]->index, true));
    Value * lengthPointerGEP = currentPackage->currentModule->builder->CreateGEP(arrayType->getInternalType(), arrayValue, {zeroValue, lengthIndexValue});
    Value * lengthValue32 = currentPackage->currentModule->builder->CreateLoad(lengthPointerGEP);

    Value * lengthValue = currentPackage->currentModule->builder->CreateIntCast(lengthValue32, llvm::Type::getInt64Ty(*currentPackage->context), true);

    Value * lengthMinusOneValue = currentPackage->currentModule->builder->CreateSub(lengthValue, ConstantInt::get(*currentPackage->context, llvm::APInt(64, 1, true)));

    auto elementSize = ConstantExpr::getSizeOf(stringType->getReferencableType());

    // Allocate N string pointers
    auto arrayMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),                       // Size type, e.g. 'n' in malloc(n)
        stringType->getReferencableType(),                                      // output type, which we get pointer to?
        elementSize,                                                            // size of each element
        lengthValue,
        nullptr,
        "");
    currentPackage->currentModule->builder->Insert(arrayMemoryPointer);

    // TODO: Create a map function that works on arrays, where toString can be the mapping

    // Create count variable
    Value * countVariable = ConstantInt::get(*currentPackage->context, llvm::APInt(64, 0, true));
    Value * countVariablePtr = currentPackage->currentModule->builder->CreateAlloca(Type::getInt64Ty(*currentPackage->context), nullptr, "elementIndex");
    currentPackage->currentModule->builder->CreateStore(countVariable, countVariablePtr);

    // Create sum variable, to hold the sum length of all strings
    Value * totalLengthVariable = ConstantInt::get(*currentPackage->context, llvm::APInt(64, 0, true));
    Value * totalLengthVariablePtr = currentPackage->currentModule->builder->CreateAlloca(Type::getInt64Ty(*currentPackage->context), nullptr, "totalLength");
    currentPackage->currentModule->builder->CreateStore(totalLengthVariable, totalLengthVariablePtr);

    // Set up branches similar to while loop, where condition is that count variable < lengthValue

    // Set up the 3 blocks we need: condition, loop-block and merge
    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();
    BasicBlock *condBlock = BasicBlock::Create(*currentPackage->context, "loopcond", function);
    BasicBlock *loopBlock = BasicBlock::Create(*currentPackage->context, "loop", function);
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->context, "afterloop", function);

    // Jump to condition
    currentPackage->currentModule->builder->CreateBr(condBlock);
    currentPackage->currentModule->builder->SetInsertPoint(condBlock);

    // while condition
    Value * existingValueCondition = currentPackage->currentModule->builder->CreateLoad(countVariablePtr);
    Value * expression = currentPackage->currentModule->builder->CreateICmpSLT(existingValueCondition, lengthValue);

    // Create the condition - if expression is true, jump to loop block, else jump to after loop block
    currentPackage->currentModule->builder->CreateCondBr(expression, loopBlock, mergeBlock);

    // Set insert point to the loop-block so we can populate it
    currentPackage->currentModule->builder->SetInsertPoint(loopBlock);

    auto memoryPointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayType->properties["memoryPointer"]->index, true));
    auto memoryPointerGEP = currentPackage->currentModule->builder->CreateGEP(arrayType->getInternalType(), arrayValue, {zeroValue, memoryPointerIndexValue});
    Value * memoryPointerValue = currentPackage->currentModule->builder->CreateLoad(memoryPointerGEP);

    // Get the correct toString-function for the generic type
    BalanceType * btype = currentPackage->currentModule->getType(arrayType->generics[0]->name);
    Function * genericToStringFunction = btype->getMethod("toString")->getLlvmFunction(currentPackage->currentModule);
    Type * genericType = btype->getReferencableType();

    // Load the i'th element
    Value * ithIndexValue = currentPackage->currentModule->builder->CreateLoad(countVariablePtr);
    auto ithElementGEP = currentPackage->currentModule->builder->CreateGEP(genericType, memoryPointerValue, {ithIndexValue});
    Value * ithElement = currentPackage->currentModule->builder->CreateLoad(ithElementGEP);

    // call toString
    Value * ithElementStringValue = currentPackage->currentModule->builder->CreateCall(genericToStringFunction, {ithElement});

    // get length of ith string
    auto stringLengthZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto stringLengthIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["length"]->index, true));
    auto ithElementLengthGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), ithElementStringValue, {stringLengthZeroValue, stringLengthIndex});
    auto ithElementLengthValue = currentPackage->currentModule->builder->CreateLoad(ithElementLengthGEP);

    // Update sum
    Value * existingSum = currentPackage->currentModule->builder->CreateLoad(totalLengthVariablePtr);
    Value * newSumValue = currentPackage->currentModule->builder->CreateAdd(existingSum, ithElementLengthValue);
    currentPackage->currentModule->builder->CreateStore(newSumValue, totalLengthVariablePtr);

    // Create GEP of ith output array index
    auto ithElementOutputGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getReferencableType(), arrayMemoryPointer, {ithIndexValue});
    // Store ith-string in ith array index
    currentPackage->currentModule->builder->CreateStore(ithElementStringValue, ithElementOutputGEP);

    // increment countvariable
    Value * existingValue = currentPackage->currentModule->builder->CreateLoad(countVariablePtr);
    Value * incrementedValue = currentPackage->currentModule->builder->CreateAdd(existingValue, ConstantInt::get(*currentPackage->context, llvm::APInt(64, 1, true)));
    currentPackage->currentModule->builder->CreateStore(incrementedValue, countVariablePtr);

    // At the end of while-block, jump back to the condition which may jump to mergeBlock or reiterate
    currentPackage->currentModule->builder->CreateBr(condBlock);

    // Make sure new code is added to "block" after while statement
    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);

    // Calculate total length including brackets, commas and whitespace (+ 2 + 2*(N-1))
    Value * totalSum = currentPackage->currentModule->builder->CreateLoad(totalLengthVariablePtr);
    auto nMinusOne = currentPackage->currentModule->builder->CreateSub(lengthValue, ConstantInt::get(*currentPackage->context, llvm::APInt(64, 1, true)));
    Value * twoValue = ConstantInt::get(*currentPackage->context, llvm::APInt(64, 2, true));
    auto twoTimesNMinusOne = currentPackage->currentModule->builder->CreateMul(twoValue, nMinusOne);
    auto plusBrackets = currentPackage->currentModule->builder->CreateAdd(twoTimesNMinusOne, twoValue);
    Value * finalSum = currentPackage->currentModule->builder->CreateAdd(totalSum, plusBrackets);

    // Calculate string length + null terminator
    Value * stringLengthWithNullTerminator = currentPackage->currentModule->builder->CreateAdd(finalSum, ConstantInt::get(*currentPackage->context, llvm::APInt(64, 1, true)));

    // Allocate memory for final string memory
    auto finalStringMemory = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),                               // input type?
        llvm::Type::getInt8Ty(*currentPackage->context),                                // output type, which we get pointer to?
        stringLengthWithNullTerminator,                                                 // size, matches input type?
        nullptr, nullptr);
    currentPackage->currentModule->builder->Insert(finalStringMemory);

    // Insert opening bracket (ASCII 91)
    Value * openingBracket = ConstantInt::get(*currentPackage->context, llvm::APInt(8, 91, true));
    Value * destinationOpeningBracket = currentPackage->currentModule->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {zeroValue});
    currentPackage->currentModule->builder->CreateStore(openingBracket, destinationOpeningBracket);

    // Copy strings into final string memory

    // Create new index variable = 0
    auto memcpyIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(64, 0, true));
    Value * memcpyIndexPtr = currentPackage->currentModule->builder->CreateAlloca(Type::getInt64Ty(*currentPackage->context));
    currentPackage->currentModule->builder->CreateStore(memcpyIndex, memcpyIndexPtr);

    // Create new currentByteOffset variable = 1 (since we already have opening bracket)
    auto currentByteOffset = ConstantInt::get(*currentPackage->context, llvm::APInt(64, 1, true));
    Value * currentByteOffsetPtr = currentPackage->currentModule->builder->CreateAlloca(Type::getInt64Ty(*currentPackage->context));
    currentPackage->currentModule->builder->CreateStore(currentByteOffset, currentByteOffsetPtr);

    // Set up the 3 blocks we need: condition, loop-block and merge
    Function *functionAtMemcpy = currentPackage->currentModule->builder->GetInsertBlock()->getParent();
    BasicBlock *memcpyCondBlock = BasicBlock::Create(*currentPackage->context, "memcpyCondBlock", functionAtMemcpy);
    BasicBlock *memcpyLoopBlock = BasicBlock::Create(*currentPackage->context, "memcpyLoopBlock", functionAtMemcpy);
    BasicBlock *memcpyMergeBlock = BasicBlock::Create(*currentPackage->context, "memcpyMergeBlock", functionAtMemcpy);

    // Jump to condition
    currentPackage->currentModule->builder->CreateBr(memcpyCondBlock);
    currentPackage->currentModule->builder->SetInsertPoint(memcpyCondBlock);

    // while condition
    Value * existingMemcpyIndex = currentPackage->currentModule->builder->CreateLoad(memcpyIndexPtr);
    // We loop N-1 times while adding ", " and then do that last "manually"
    Value * memcpyCondition = currentPackage->currentModule->builder->CreateICmpSLT(existingMemcpyIndex, lengthMinusOneValue);

    // Create the condition - if expression is true, jump to loop block, else jump to after loop block
    currentPackage->currentModule->builder->CreateCondBr(memcpyCondition, memcpyLoopBlock, memcpyMergeBlock);

    // Set insert point to the loop-block so we can populate it
    currentPackage->currentModule->builder->SetInsertPoint(memcpyLoopBlock);

    // Load current index
    Value * existingMemcpyIndexInLoop = currentPackage->currentModule->builder->CreateLoad(memcpyIndexPtr);

    // Load current byteOffset
    Value * existingByteOffsetInLoop = currentPackage->currentModule->builder->CreateLoad(currentByteOffsetPtr);

    // Load current string
    auto ithMemcpyIndexValueGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getReferencableType(), arrayMemoryPointer, {existingMemcpyIndexInLoop});
    Value * ithMemcpyElement = currentPackage->currentModule->builder->CreateLoad(ithMemcpyIndexValueGEP);

    // Get GEP for current string length and load
    auto currentStringLengthZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto currentStringLengthIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["length"]->index, true));
    auto currentStringLengthGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), ithMemcpyElement, {currentStringLengthZeroValue, currentStringLengthIndexValue});
    Value * currentStringLengthValue = currentPackage->currentModule->builder->CreateLoad(currentStringLengthGEP);

    // Get GEP for current string memory
    auto currentStringMemoryZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto currentStringMemoryIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["stringPointer"]->index, true));
    auto currentStringMemoryGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), ithMemcpyElement, {currentStringMemoryZeroValue, currentStringMemoryIndexValue});
    Value * currentStringMemoryValue = currentPackage->currentModule->builder->CreateLoad(currentStringMemoryGEP);

    // Calculate destination address + byteOffset
    Value * destinationAddress = currentPackage->currentModule->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {existingByteOffsetInLoop});

    // Create call to memcpy with destination (with offset), source and num
    // void * memcpy ( void * destination, const void * source, size_t num );
    Function * memcpyFunc = currentPackage->currentModule->module->getFunction("memcpy");
    currentPackage->currentModule->builder->CreateCall(memcpyFunc, {destinationAddress, currentStringMemoryValue, currentStringLengthValue});

    // Update byteOffset as current byteOffset + current string length
    Value * byteOffsetPlusString = currentPackage->currentModule->builder->CreateAdd(existingByteOffsetInLoop, currentStringLengthValue);

    // Add "," (44)
    Value * comma = ConstantInt::get(*currentPackage->context, llvm::APInt(8, 44, true));
    Value * destinationComma = currentPackage->currentModule->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {byteOffsetPlusString});
    currentPackage->currentModule->builder->CreateStore(comma, destinationComma);

    Value * oneValue = ConstantInt::get(*currentPackage->context, llvm::APInt(64, 1, true));
    Value * byteOffsetPlusComma = currentPackage->currentModule->builder->CreateAdd(byteOffsetPlusString, oneValue);

    // Add " " (32)
    Value * whiteSpace = ConstantInt::get(*currentPackage->context, llvm::APInt(8, 32, true));
    Value * destinationwhiteSpace = currentPackage->currentModule->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {byteOffsetPlusComma});
    currentPackage->currentModule->builder->CreateStore(whiteSpace, destinationwhiteSpace);

    Value * byteOffsetPlusWhiteSpace = currentPackage->currentModule->builder->CreateAdd(byteOffsetPlusComma, oneValue);

    // Store new byteoffset
    currentPackage->currentModule->builder->CreateStore(byteOffsetPlusWhiteSpace, currentByteOffsetPtr);

    // Increment index + 1
    Value * incrementedMemcpyIndexInLoop = currentPackage->currentModule->builder->CreateAdd(existingMemcpyIndexInLoop, ConstantInt::get(*currentPackage->context, llvm::APInt(64, 1, true)));
    // Store new index
    currentPackage->currentModule->builder->CreateStore(incrementedMemcpyIndexInLoop, memcpyIndexPtr);

    // At the end of while-block, jump back to the condition which may jump to mergeBlock or reiterate
    currentPackage->currentModule->builder->CreateBr(memcpyCondBlock);

    // Make sure new code is added to "block" after while statement
    currentPackage->currentModule->builder->SetInsertPoint(memcpyMergeBlock);

    // BEGIN LAST ELEMENT
    // Load current index
    Value * lastElementIndex = currentPackage->currentModule->builder->CreateLoad(memcpyIndexPtr);

    // Load current byteOffset
    Value * lastElementByteOffset = currentPackage->currentModule->builder->CreateLoad(currentByteOffsetPtr);

    // Load current string
    auto lastElementGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getReferencableType(), arrayMemoryPointer, {lastElementIndex});
    Value * lastElement = currentPackage->currentModule->builder->CreateLoad(lastElementGEP);

    // Get GEP for current string length and load
    auto lastElementZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto lastElementLengthIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["length"]->index, true));
    auto lastElementLengthGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), lastElement, {lastElementZeroValue, lastElementLengthIndexValue});
    Value * lastElementLengthValue = currentPackage->currentModule->builder->CreateLoad(lastElementLengthGEP);

    // Get GEP for current string memory
    auto lastElementMemoryZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto lastElementMemoryIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["stringPointer"]->index, true));
    auto lastElementMemoryGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), lastElement, {lastElementMemoryZeroValue, lastElementMemoryIndexValue});
    Value * lastElementMemoryValue = currentPackage->currentModule->builder->CreateLoad(lastElementMemoryGEP);

    // Calculate destination address + byteOffset
    Value * lastElementDestinationAddress = currentPackage->currentModule->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {lastElementByteOffset});

    // Create call to memcpy with destination (with offset), source and num
    // void * memcpy ( void * destination, const void * source, size_t num );
    currentPackage->currentModule->builder->CreateCall(memcpyFunc, {lastElementDestinationAddress, lastElementMemoryValue, lastElementLengthValue});

    // Update byteOffset as current byteOffset + current string length
    Value * lastOffset = currentPackage->currentModule->builder->CreateAdd(lastElementByteOffset, lastElementLengthValue);
    // END LAST ELEMENT

    // Insert closing bracket (ASCII 93)
    Value * closingBracket = ConstantInt::get(*currentPackage->context, llvm::APInt(8, 93, true));
    Value * destinationClosingBracket = currentPackage->currentModule->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {lastOffset});
    currentPackage->currentModule->builder->CreateStore(closingBracket, destinationClosingBracket);

    // Add null-terminator
    auto zeroConstant = ConstantInt::get(*currentPackage->context, llvm::APInt(8, 0, true));
    Value * nullTerminatorAddress = currentPackage->currentModule->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, { finalSum });
    currentPackage->currentModule->builder->CreateStore(zeroConstant, nullTerminatorAddress);

    // Create string and set pointer + size
    auto outputStringPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),           // input type?
        stringType->getInternalType(),                              // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringType->getInternalType()),     // size, matches input type?
        nullptr,
        nullptr,
        ""
    );
    currentPackage->currentModule->builder->Insert(outputStringPointer);

    int outputStringPointerIndex = stringType->properties["stringPointer"]->index;
    auto outputStringPointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto outputStringPointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, outputStringPointerIndex, true));
    auto outputStringGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), outputStringPointer, {outputStringPointerZeroValue, outputStringPointerIndexValue});

    currentPackage->currentModule->builder->CreateStore(finalStringMemory, outputStringGEP);

    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["length"]->index, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), outputStringPointer, {sizeZeroValue, sizeIndexValue});
    currentPackage->currentModule->builder->CreateStore(finalSum, sizeGEP);

    currentPackage->currentModule->builder->CreateRet(outputStringPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);

    bool hasError = verifyFunction(*toStringFunction->getLlvmFunction(currentPackage->currentModule), &llvm::errs());
    if (hasError) {
        toStringFunction->getLlvmFunction(currentPackage->currentModule)->print(llvm::errs(), nullptr);
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        throw std::runtime_error("Error verifying function: " + toStringFunction->name);
    }
}
