#include "Array.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "../models/BalanceType.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createMethod_Array_toString(BalanceType * arrayType) {
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

    BalanceParameter * valueParameter = new BalanceParameter(arrayType, "value");

    std::string functionName = "toString";
    std::string functionNameWithClass = arrayType->toString() + "_" + functionName;
    BalanceType * stringType = currentPackage->currentModule->getType("String");

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        arrayType->getReferencableType()
    });

    FunctionType *functionType = FunctionType::get(stringType->getReferencableType(), parametersReference, false);
    llvm::Function * arrayToStringFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->builtins->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", arrayToStringFunc);

    BalanceFunction * bfunction = new BalanceFunction(functionName, parameters, stringType);
    arrayType->addMethod(functionName, bfunction);
    bfunction->function = arrayToStringFunc;

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->builtins->builder->GetInsertBlock();
    currentPackage->builtins->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = arrayToStringFunc->arg_begin();
    Value * arrayValue = args++;

    // Get length (N)
    ConstantInt * zeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    ConstantInt * lengthIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayType->properties["length"]->index, true));
    Value * lengthPointerGEP = currentPackage->builtins->builder->CreateGEP(arrayType->getInternalType(), arrayValue, {zeroValue, lengthIndexValue});
    Value * lengthValue = currentPackage->builtins->builder->CreateLoad(lengthPointerGEP);

    Value * lengthMinusOneValue = currentPackage->builtins->builder->CreateSub(lengthValue, ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)));

    auto elementSize = ConstantExpr::getSizeOf(stringType->getReferencableType());

    // Allocate N string pointers
    auto arrayMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->builtins->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),                       // Size type, e.g. 'n' in malloc(n)
        stringType->getReferencableType(),                                      // output type, which we get pointer to?
        elementSize,                                                            // size of each element
        lengthValue,
        nullptr,
        "");
    currentPackage->builtins->builder->Insert(arrayMemoryPointer);

    // TODO: Create a map function that works on arrays, where toString can be the mapping

    // Create count variable
    Value * countVariable = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * countVariablePtr = currentPackage->builtins->builder->CreateAlloca(Type::getInt32Ty(*currentPackage->context), nullptr, "elementIndex");
    currentPackage->builtins->builder->CreateStore(countVariable, countVariablePtr);

    // Create sum variable, to hold the sum length of all strings
    Value * totalLengthVariable = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * totalLengthVariablePtr = currentPackage->builtins->builder->CreateAlloca(Type::getInt32Ty(*currentPackage->context), nullptr, "totalLength");
    currentPackage->builtins->builder->CreateStore(totalLengthVariable, totalLengthVariablePtr);

    // Set up branches similar to while loop, where condition is that count variable < lengthValue

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

    auto memoryPointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayType->properties["memoryPointer"]->index, true));
    auto memoryPointerGEP = currentPackage->builtins->builder->CreateGEP(arrayType->getInternalType(), arrayValue, {zeroValue, memoryPointerIndexValue});
    Value * memoryPointerValue = currentPackage->builtins->builder->CreateLoad(memoryPointerGEP);

    // Get the correct toString-function for the generic type
    BalanceType * btype = currentPackage->currentModule->getType(arrayType->generics[0]->name);
    Function * genericToStringFunction = btype->getMethod("toString")->function;
    Type * genericType = btype->getReferencableType();

    // Load the i'th element
    Value * ithIndexValue = currentPackage->builtins->builder->CreateLoad(countVariablePtr);
    auto ithElementGEP = currentPackage->builtins->builder->CreateGEP(genericType, memoryPointerValue, {ithIndexValue});
    Value * ithElement = currentPackage->builtins->builder->CreateLoad(ithElementGEP);

    // call toString
    ArrayRef<Value *> toStringArguments{ithElement};
    Value * ithElementStringValue = currentPackage->builtins->builder->CreateCall(genericToStringFunction, toStringArguments);

    // get length of ith string
    auto stringLengthZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto stringLengthIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["length"]->index, true));
    auto ithElementLengthGEP = currentPackage->builtins->builder->CreateGEP(stringType->getInternalType(), ithElementStringValue, {stringLengthZeroValue, stringLengthIndex});
    auto ithElementLengthValue = currentPackage->builtins->builder->CreateLoad(ithElementLengthGEP);

    // Update sum
    Value * existingSum = currentPackage->builtins->builder->CreateLoad(totalLengthVariablePtr);
    Value * newSumValue = currentPackage->builtins->builder->CreateAdd(existingSum, ithElementLengthValue);
    currentPackage->builtins->builder->CreateStore(newSumValue, totalLengthVariablePtr);

    // Create GEP of ith output array index
    auto ithElementOutputGEP = currentPackage->builtins->builder->CreateGEP(stringType->getReferencableType(), arrayMemoryPointer, {ithIndexValue});
    // Store ith-string in ith array index
    currentPackage->builtins->builder->CreateStore(ithElementStringValue, ithElementOutputGEP);

    // increment countvariable
    Value * existingValue = currentPackage->builtins->builder->CreateLoad(countVariablePtr);
    Value * incrementedValue = currentPackage->builtins->builder->CreateAdd(existingValue, ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)));
    currentPackage->builtins->builder->CreateStore(incrementedValue, countVariablePtr);

    // At the end of while-block, jump back to the condition which may jump to mergeBlock or reiterate
    currentPackage->builtins->builder->CreateBr(condBlock);

    // Make sure new code is added to "block" after while statement
    currentPackage->builtins->builder->SetInsertPoint(mergeBlock);

    // Calculate total length including brackets, commas and whitespace (+ 2 + 2*(N-1))
    Value * totalSum = currentPackage->builtins->builder->CreateLoad(totalLengthVariablePtr);
    auto nMinusOne = currentPackage->builtins->builder->CreateSub(lengthValue, ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)));
    Value * twoValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 2, true));
    auto twoTimesNMinusOne = currentPackage->builtins->builder->CreateMul(twoValue, nMinusOne);
    auto plusBrackets = currentPackage->builtins->builder->CreateAdd(twoTimesNMinusOne, twoValue);
    Value * finalSum = currentPackage->builtins->builder->CreateAdd(totalSum, plusBrackets);

    // Calculate string length + null terminator
    Value * stringLengthWithNullTerminator = currentPackage->builtins->builder->CreateAdd(finalSum, ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)));

    // Allocate memory for final string memory
    auto finalStringMemory = llvm::CallInst::CreateMalloc(
        currentPackage->builtins->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),                               // input type?
        llvm::Type::getInt8Ty(*currentPackage->context),                                // output type, which we get pointer to?
        stringLengthWithNullTerminator,                                                 // size, matches input type?
        nullptr, nullptr);
    currentPackage->builtins->builder->Insert(finalStringMemory);

    // Insert opening bracket (ASCII 91)
    Value * openingBracket = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 91, true));
    Value * destinationOpeningBracket = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {zeroValue});
    currentPackage->builtins->builder->CreateStore(openingBracket, destinationOpeningBracket);

    // Copy strings into final string memory

    // Create new index variable = 0
    auto memcpyIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * memcpyIndexPtr = currentPackage->builtins->builder->CreateAlloca(Type::getInt32Ty(*currentPackage->context));
    currentPackage->builtins->builder->CreateStore(memcpyIndex, memcpyIndexPtr);

    // Create new currentByteOffset variable = 1 (since we already have opening bracket)
    auto currentByteOffset = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true));
    Value * currentByteOffsetPtr = currentPackage->builtins->builder->CreateAlloca(Type::getInt32Ty(*currentPackage->context));
    currentPackage->builtins->builder->CreateStore(currentByteOffset, currentByteOffsetPtr);

    // Set up the 3 blocks we need: condition, loop-block and merge
    Function *functionAtMemcpy = currentPackage->builtins->builder->GetInsertBlock()->getParent();
    BasicBlock *memcpyCondBlock = BasicBlock::Create(*currentPackage->context, "memcpyCondBlock", functionAtMemcpy);
    BasicBlock *memcpyLoopBlock = BasicBlock::Create(*currentPackage->context, "memcpyLoopBlock", functionAtMemcpy);
    BasicBlock *memcpyMergeBlock = BasicBlock::Create(*currentPackage->context, "memcpyMergeBlock", functionAtMemcpy);

    // Jump to condition
    currentPackage->builtins->builder->CreateBr(memcpyCondBlock);
    currentPackage->builtins->builder->SetInsertPoint(memcpyCondBlock);

    // while condition
    Value * existingMemcpyIndex = currentPackage->builtins->builder->CreateLoad(memcpyIndexPtr);
    // We loop N-1 times while adding ", " and then do that last "manually"
    Value * memcpyCondition = currentPackage->builtins->builder->CreateICmpSLT(existingMemcpyIndex, lengthMinusOneValue);

    // Create the condition - if expression is true, jump to loop block, else jump to after loop block
    currentPackage->builtins->builder->CreateCondBr(memcpyCondition, memcpyLoopBlock, memcpyMergeBlock);

    // Set insert point to the loop-block so we can populate it
    currentPackage->builtins->builder->SetInsertPoint(memcpyLoopBlock);

    // Load current index
    Value * existingMemcpyIndexInLoop = currentPackage->builtins->builder->CreateLoad(memcpyIndexPtr);

    // Load current byteOffset
    Value * existingByteOffsetInLoop = currentPackage->builtins->builder->CreateLoad(currentByteOffsetPtr);

    // Load current string
    auto ithMemcpyIndexValueGEP = currentPackage->builtins->builder->CreateGEP(stringType->getReferencableType(), arrayMemoryPointer, {existingMemcpyIndexInLoop});
    Value * ithMemcpyElement = currentPackage->builtins->builder->CreateLoad(ithMemcpyIndexValueGEP);

    // Get GEP for current string length and load
    auto currentStringLengthZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto currentStringLengthIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["length"]->index, true));
    auto currentStringLengthGEP = currentPackage->builtins->builder->CreateGEP(stringType->getInternalType(), ithMemcpyElement, {currentStringLengthZeroValue, currentStringLengthIndexValue});
    Value * currentStringLengthValue = currentPackage->builtins->builder->CreateLoad(currentStringLengthGEP);

    // Get GEP for current string memory
    auto currentStringMemoryZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto currentStringMemoryIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["stringPointer"]->index, true));
    auto currentStringMemoryGEP = currentPackage->builtins->builder->CreateGEP(stringType->getInternalType(), ithMemcpyElement, {currentStringMemoryZeroValue, currentStringMemoryIndexValue});
    Value * currentStringMemoryValue = currentPackage->builtins->builder->CreateLoad(currentStringMemoryGEP);

    // Calculate destination address + byteOffset
    Value * destinationAddress = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {existingByteOffsetInLoop});

    // Create call to memcpy with destination (with offset), source and num
    // void * memcpy ( void * destination, const void * source, size_t num );
    ArrayRef<Value *> memcpyArguments {destinationAddress, currentStringMemoryValue, currentStringLengthValue};
    Function * memcpyFunc = currentPackage->builtins->module->getFunction("memcpy");
    currentPackage->builtins->builder->CreateCall(memcpyFunc, memcpyArguments);

    // Update byteOffset as current byteOffset + current string length
    Value * byteOffsetPlusString = currentPackage->builtins->builder->CreateAdd(existingByteOffsetInLoop, currentStringLengthValue);

    // Add "," (44)
    Value * comma = ConstantInt::get(*currentPackage->context, llvm::APInt(8, 44, true));
    Value * destinationComma = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {byteOffsetPlusString});
    currentPackage->builtins->builder->CreateStore(comma, destinationComma);

    Value * oneValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true));
    Value * byteOffsetPlusComma = currentPackage->builtins->builder->CreateAdd(byteOffsetPlusString, oneValue);

    // Add " " (32)
    Value * whiteSpace = ConstantInt::get(*currentPackage->context, llvm::APInt(8, 32, true));
    Value * destinationwhiteSpace = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {byteOffsetPlusComma});
    currentPackage->builtins->builder->CreateStore(whiteSpace, destinationwhiteSpace);

    Value * byteOffsetPlusWhiteSpace = currentPackage->builtins->builder->CreateAdd(byteOffsetPlusComma, oneValue);

    // Store new byteoffset
    currentPackage->builtins->builder->CreateStore(byteOffsetPlusWhiteSpace, currentByteOffsetPtr);

    // Increment index + 1
    Value * incrementedMemcpyIndexInLoop = currentPackage->builtins->builder->CreateAdd(existingMemcpyIndexInLoop, ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)));
    // Store new index
    currentPackage->builtins->builder->CreateStore(incrementedMemcpyIndexInLoop, memcpyIndexPtr);

    // At the end of while-block, jump back to the condition which may jump to mergeBlock or reiterate
    currentPackage->builtins->builder->CreateBr(memcpyCondBlock);

    // Make sure new code is added to "block" after while statement
    currentPackage->builtins->builder->SetInsertPoint(memcpyMergeBlock);

    // BEGIN LAST ELEMENT
    // Load current index
    Value * lastElementIndex = currentPackage->builtins->builder->CreateLoad(memcpyIndexPtr);

    // Load current byteOffset
    Value * lastElementByteOffset = currentPackage->builtins->builder->CreateLoad(currentByteOffsetPtr);

    // Load current string
    auto lastElementGEP = currentPackage->builtins->builder->CreateGEP(stringType->getReferencableType(), arrayMemoryPointer, {lastElementIndex});
    Value * lastElement = currentPackage->builtins->builder->CreateLoad(lastElementGEP);

    // Get GEP for current string length and load
    auto lastElementZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto lastElementLengthIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["length"]->index, true));
    auto lastElementLengthGEP = currentPackage->builtins->builder->CreateGEP(stringType->getInternalType(), lastElement, {lastElementZeroValue, lastElementLengthIndexValue});
    Value * lastElementLengthValue = currentPackage->builtins->builder->CreateLoad(lastElementLengthGEP);

    // Get GEP for current string memory
    auto lastElementMemoryZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto lastElementMemoryIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["stringPointer"]->index, true));
    auto lastElementMemoryGEP = currentPackage->builtins->builder->CreateGEP(stringType->getInternalType(), lastElement, {lastElementMemoryZeroValue, lastElementMemoryIndexValue});
    Value * lastElementMemoryValue = currentPackage->builtins->builder->CreateLoad(lastElementMemoryGEP);

    // Calculate destination address + byteOffset
    Value * lastElementDestinationAddress = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {lastElementByteOffset});

    // Create call to memcpy with destination (with offset), source and num
    // void * memcpy ( void * destination, const void * source, size_t num );
    ArrayRef<Value *> lastElementMemcpyArguments {lastElementDestinationAddress, lastElementMemoryValue, lastElementLengthValue};
    currentPackage->builtins->builder->CreateCall(memcpyFunc, lastElementMemcpyArguments);

    // Update byteOffset as current byteOffset + current string length
    Value * lastOffset = currentPackage->builtins->builder->CreateAdd(lastElementByteOffset, lastElementLengthValue);
    // END LAST ELEMENT

    auto one = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true));

    // Insert closing bracket (ASCII 93)
    Value * closingBracket = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 93, true));
    Value * destinationClosingBracket = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, {lastOffset});
    currentPackage->builtins->builder->CreateStore(closingBracket, destinationClosingBracket);

    // Add null-terminator
    auto zeroConstant = ConstantInt::get(*currentPackage->context, llvm::APInt(8, 0, true));
    Value * nullTerminatorAddress = currentPackage->builtins->builder->CreateGEP(Type::getInt8Ty(*currentPackage->context), finalStringMemory, { finalSum });
    currentPackage->builtins->builder->CreateStore(zeroConstant, nullTerminatorAddress);

    // Create string and set pointer + size
    auto outputStringPointer = llvm::CallInst::CreateMalloc(
        currentPackage->builtins->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),           // input type?
        stringType->getInternalType(),                              // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringType->getInternalType()),     // size, matches input type?
        nullptr,
        nullptr,
        ""
    );
    currentPackage->builtins->builder->Insert(outputStringPointer);

    int outputStringPointerIndex = stringType->properties["stringPointer"]->index;
    auto outputStringPointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto outputStringPointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, outputStringPointerIndex, true));
    auto outputStringGEP = currentPackage->builtins->builder->CreateGEP(stringType->getInternalType(), outputStringPointer, {outputStringPointerZeroValue, outputStringPointerIndexValue});

    currentPackage->builtins->builder->CreateStore(finalStringMemory, outputStringGEP);

    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["length"]->index, true));
    auto sizeGEP = currentPackage->builtins->builder->CreateGEP(stringType->getInternalType(), outputStringPointer, {sizeZeroValue, sizeIndexValue});
    currentPackage->builtins->builder->CreateStore(finalSum, sizeGEP);

    currentPackage->builtins->builder->CreateRet(outputStringPointer);
    currentPackage->builtins->builder->SetInsertPoint(resumeBlock);
}

BalanceType * createType__Array(BalanceType * generic) {
    BalanceType * arrayType = new BalanceType(currentPackage->currentModule, "Array", {generic});

    if (generic == nullptr) {
        // This registers the base type, which can't be instantiated directly.
        currentPackage->builtins->genericTypes["Array"] = arrayType;
        return arrayType;
    }

    currentPackage->builtins->addType(arrayType);
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

    createDefaultConstructor(currentPackage->builtins, arrayType);

    createMethod_Array_toString(arrayType);

    return arrayType;
}