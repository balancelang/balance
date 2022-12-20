#include "Bool.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "../models/BalanceTypeString.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createMethod_Bool_toString() {
    BalanceParameter * valueParameter = new BalanceParameter(new BalanceTypeString("Bool"), "value");
    valueParameter->type = getBuiltinType(new BalanceTypeString("Bool"));

    std::string functionName = "toString";
    std::string functionNameWithClass = "Bool_" + functionName;

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        getBuiltinType(new BalanceTypeString("Bool"))
    });

    FunctionType *functionType = FunctionType::get(getBuiltinType(new BalanceTypeString("String")), parametersReference, false);

    llvm::Function * boolToStringFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", boolToStringFunc);

    BalanceFunction * bfunction = new BalanceFunction(functionName, parameters, new BalanceTypeString("String"));
    currentPackage->currentModule->currentClass->addMethod(functionName, bfunction);
    bfunction->function = boolToStringFunc;
    bfunction->returnType = getBuiltinType(new BalanceTypeString("String"));

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = boolToStringFunc->arg_begin();
    llvm::Value * boolValue = args++;

    BalanceType * stringType = currentPackage->currentModule->getType(new BalanceTypeString("String"));

    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringType->getInternalType(),                         // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringType->getInternalType()),// size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> argumentsReference{stringMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(stringType->getConstructor(), argumentsReference);
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
    Value * sizeValueFalse = (Value *) ConstantInt::get(IntegerType::getInt32Ty(*currentPackage->context), 5, true);
    currentPackage->currentModule->builder->CreateStore(sizeValueFalse, sizeGEP);
    Value * arrayValueFalse = geti8StrVal(*currentPackage->currentModule->module, "false", "false", true);
    currentPackage->currentModule->builder->CreateStore(arrayValueFalse, pointerGEP);

    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    thenBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(elseBlock);

    // if value != 0 (true)                                                                         // 4 = 'true'
    Value * sizeValueTrue = (Value *) ConstantInt::get(IntegerType::getInt32Ty(*currentPackage->context), 4, true);
    currentPackage->currentModule->builder->CreateStore(sizeValueTrue, sizeGEP);

    Value * arrayValueTrue = geti8StrVal(*currentPackage->currentModule->module, "true", "true", true);
    currentPackage->currentModule->builder->CreateStore(arrayValueTrue, pointerGEP);

    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    elseBlock = currentPackage->currentModule->builder->GetInsertBlock();

    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    currentPackage->currentModule->builder->CreateRet(stringMemoryPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createType__Bool() {
    auto typeString = new BalanceTypeString("Bool");
    BalanceClass * bclass = new BalanceClass(typeString, currentPackage->currentModule);
    bclass->internalType = getBuiltinType(typeString);
    bclass->isSimpleType = true;

    currentPackage->currentModule->classes["Bool"] = bclass;
    currentPackage->currentModule->currentClass = bclass;

    createMethod_Bool_toString();

    currentPackage->currentModule->currentClass = nullptr;
}