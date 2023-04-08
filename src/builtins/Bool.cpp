#include "Bool.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "../models/BalanceType.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void BoolType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Bool");
    this->balanceType->isSimpleType = true;
    currentPackage->currentModule->addType(this->balanceType);
}

void BoolType::finalizeType() {
    this->balanceType->internalType = Type::getInt1Ty(*currentPackage->context);
}

void BoolType::registerMethods() {
    this->registerMethod_toString();
}

void BoolType::finalizeMethods() {
    this->finalizeMethod_toString();
}

void BoolType::registerFunctions() {

}

void BoolType::finalizeFunctions() {

}

void BoolType::registerMethod_toString() {
    std::string functionName = "toString";
    BalanceType * stringType = currentPackage->currentModule->getType("String");

    BalanceParameter * valueParameter = new BalanceParameter(this->balanceType, "value");
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };
    BalanceFunction * bfunction = new BalanceFunction(currentPackage->currentModule, this->balanceType, functionName, parameters, stringType);
    this->balanceType->addMethod(functionName, bfunction);
}

void BoolType::finalizeMethod_toString() {
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    BalanceFunction * toStringFunction = this->balanceType->getMethod("toString");
    BalanceType * boolType = currentPackage->currentModule->getType("Bool");

    ArrayRef<Type *> parametersReference({
        boolType->getReferencableType()
    });

    BalanceParameter * valueParameter = new BalanceParameter(boolType, "value");

    std::string functionName = "toString";
    std::string functionNameWithClass = "Bool_" + functionName;

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };

    // Create llvm::Function

    llvm::Function * boolToStringFunc = Function::Create(toStringFunction->getLlvmFunctionType(), Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", boolToStringFunc);

    toStringFunction->setLlvmFunction(boolToStringFunc);

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = boolToStringFunc->arg_begin();
    llvm::Value * boolValue = args++;

    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringType->getInternalType(),                         // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringType->getInternalType()),// size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> argumentsReference{stringMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(stringType->getInitializer()->getLlvmFunction(currentPackage->currentModule), argumentsReference);
    int pointerIndex = stringType->properties["stringPointer"]->index;
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, pointerIndex, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), stringMemoryPointer, {pointerZeroValue, pointerIndexValue});
    int sizeIndex = stringType->properties["length"]->index;
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, sizeIndex, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), stringMemoryPointer, {sizeZeroValue, sizeIndexValue});

    ArrayRef<Value *> arguments({
        stringMemoryPointer,
    });

    // Create if-statement to print 'true' or 'false'
    BasicBlock *thenBlock = BasicBlock::Create(*currentPackage->context, "then", boolToStringFunc);
    BasicBlock *elseBlock = BasicBlock::Create(*currentPackage->context, "else", boolToStringFunc);
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->context, "ifcont", boolToStringFunc);

    Value * zero = ConstantInt::get(*currentPackage->context, llvm::APInt(1, 0, true));
    Value * expression = currentPackage->currentModule->builder->CreateICmpEQ(zero, boolValue);

    currentPackage->currentModule->builder->CreateCondBr(expression, thenBlock, elseBlock);

    currentPackage->currentModule->builder->SetInsertPoint(thenBlock);
    // if value == 0 (false)                                                                        // 5 = 'false'
    Value * sizeValueFalse = (Value *) ConstantInt::get(IntegerType::getInt64Ty(*currentPackage->context), 5, true);
    currentPackage->currentModule->builder->CreateStore(sizeValueFalse, sizeGEP);
    Value * arrayValueFalse = geti8StrVal(*currentPackage->currentModule->module, "false", "false", true);
    currentPackage->currentModule->builder->CreateStore(arrayValueFalse, pointerGEP);

    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    thenBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(elseBlock);

    // if value != 0 (true)                                                                         // 4 = 'true'
    Value * sizeValueTrue = (Value *) ConstantInt::get(IntegerType::getInt64Ty(*currentPackage->context), 4, true);
    currentPackage->currentModule->builder->CreateStore(sizeValueTrue, sizeGEP);

    Value * arrayValueTrue = geti8StrVal(*currentPackage->currentModule->module, "true", "true", true);
    currentPackage->currentModule->builder->CreateStore(arrayValueTrue, pointerGEP);

    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    elseBlock = currentPackage->currentModule->builder->GetInsertBlock();

    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    currentPackage->currentModule->builder->CreateRet(stringMemoryPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);

}
