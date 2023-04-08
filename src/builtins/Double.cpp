#include "Bool.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void DoubleType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Double");
    this->balanceType->isSimpleType = true;
    currentPackage->currentModule->addType(this->balanceType);
}

void DoubleType::finalizeType() {
    this->balanceType->internalType = Type::getDoubleTy(*currentPackage->context);
}

void DoubleType::registerMethods() {
    this->registerMethod_toString();
}

void DoubleType::finalizeMethods() {
    this->finalizeMethod_toString();
}

void DoubleType::registerFunctions() {

}

void DoubleType::finalizeFunctions() {

}

void DoubleType::registerMethod_toString() {
    std::string functionName = "toString";
    BalanceType * stringType = currentPackage->currentModule->getType("String");

    BalanceParameter * valueParameter = new BalanceParameter(this->balanceType, "value");
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };
    BalanceFunction * bfunction = new BalanceFunction(currentPackage->currentModule, this->balanceType, functionName, parameters, stringType);
    this->balanceType->addMethod(functionName, bfunction);
}

void DoubleType::finalizeMethod_toString() {
    BalanceFunction * toStringFunction = this->balanceType->getMethod("toString");

    BalanceType * doubleType = currentPackage->currentModule->getType("Double");
    BalanceType * stringType = currentPackage->currentModule->getType("String");

    // Create forward declaration of snprintf
    ArrayRef<Type *> snprintfArguments({
        llvm::Type::getInt8PtrTy(*currentPackage->context),
        llvm::IntegerType::getInt64Ty(*currentPackage->context),
        llvm::Type::getInt8PtrTy(*currentPackage->context)
    });
    llvm::FunctionType * snprintfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt64Ty(*currentPackage->context), snprintfArguments, true);
    FunctionCallee snprintfFunction = currentPackage->currentModule->module->getOrInsertFunction("snprintf", snprintfFunctionType);

    // Create llvm::Function
    llvm::Function * doubleToStringFunc = Function::Create(toStringFunction->getLlvmFunctionType(), Function::ExternalLinkage, toStringFunction->getFullyQualifiedFunctionName(), currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, toStringFunction->getFunctionName() + "_body", doubleToStringFunc);

    toStringFunction->setLlvmFunction(doubleToStringFunc);

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = doubleToStringFunc->arg_begin();
    llvm::Value * intValue = args++;

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

    // Calculate length of string with int length = snprintf(NULL, 0,"%g",42);
    ArrayRef<Value *> sizeArguments({
        ConstantPointerNull::get(Type::getInt8PtrTy(*currentPackage->context)),
        ConstantInt::get(*currentPackage->context, APInt(64, 0)),
        geti8StrVal(*currentPackage->currentModule->module, "%g", "args", true),
        intValue
    });
    Value * stringLength = currentPackage->currentModule->builder->CreateCall(snprintfFunction, sizeArguments);
    currentPackage->currentModule->builder->CreateStore(stringLength, sizeGEP);

    // TODO: stringLength + 1?
    auto memoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),   // input type?
        llvm::Type::getInt8Ty(*currentPackage->context),    // output type, which we get pointer to?
        stringLength,                                       // size, matches input type?
        nullptr,
        nullptr,
        ""
    );
    currentPackage->currentModule->builder->Insert(memoryPointer);

    // int snprintf ( char * s, size_t n, const char * format, ... );
    ArrayRef<Value *> arguments({
        memoryPointer,
        ConstantInt::get(*currentPackage->context, APInt(64, 50)),
        geti8StrVal(*currentPackage->currentModule->module, "%g", "args", true),
        intValue
    });

    currentPackage->currentModule->builder->CreateCall(snprintfFunction, arguments);
    currentPackage->currentModule->builder->CreateStore(memoryPointer, pointerGEP);

    currentPackage->currentModule->builder->CreateRet(stringMemoryPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}
