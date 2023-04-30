#include "Int.h"
#include "../Builtins.h"
#include "../BalancePackage.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void IntType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Int");
    this->balanceType->isSimpleType = true;
    currentPackage->currentModule->addType(this->balanceType);
}

void IntType::finalizeType() {
    this->balanceType->internalType = Type::getInt32Ty(*currentPackage->context);
}

void IntType::registerMethods() {
    this->registerMethod_toString();
}

void IntType::finalizeMethods() {
    this->finalizeMethod_toString();
}

void IntType::registerFunctions() {

}

void IntType::finalizeFunctions() {

}

void IntType::registerMethod_toString() {
    std::string functionName = "toString";
    BalanceType * stringType = currentPackage->currentModule->getType("String");

    BalanceParameter * valueParameter = new BalanceParameter(this->balanceType, "value");
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };
    BalanceFunction * bfunction = new BalanceFunction(currentPackage->currentModule, this->balanceType, functionName, parameters, stringType);
    this->balanceType->addMethod(bfunction);
}

void IntType::finalizeMethod_toString() {
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    BalanceFunction * toStringFunction = this->balanceType->getMethodsByName("toString")[0];

    // Create forward declaration of snprintf
    ArrayRef<Type *> snprintfArguments({
        llvm::Type::getInt8PtrTy(*currentPackage->context),
        llvm::IntegerType::getInt64Ty(*currentPackage->context),
        llvm::Type::getInt8PtrTy(*currentPackage->context)
    });
    llvm::FunctionType * snprintfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt64Ty(*currentPackage->context), snprintfArguments, true);
    FunctionCallee snprintfFunction = currentPackage->currentModule->module->getOrInsertFunction("snprintf", snprintfFunctionType);

    llvm::Function * intToStringFunc = Function::Create(toStringFunction->getLlvmFunctionType(), Function::ExternalLinkage, toStringFunction->getFullyQualifiedFunctionName(), currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, toStringFunction->getFunctionName() + "_body", intToStringFunc);

    toStringFunction->setLlvmFunction(intToStringFunc);

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = intToStringFunc->arg_begin();
    llvm::Value * intValue = args++;

    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringType->getInternalType(),                          // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringType->getInternalType()), // size, matches input type?
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
        geti8StrVal(*currentPackage->currentModule->module, "%d", "args", true),
        intValue
    });
    Value * stringLength = currentPackage->currentModule->builder->CreateCall(snprintfFunction, sizeArguments);
    currentPackage->currentModule->builder->CreateStore(stringLength, sizeGEP);

    Value * stringLengthWithNull = currentPackage->currentModule->builder->CreateAdd(stringLength, ConstantInt::get(*currentPackage->context, APInt(64, 1)));

    auto memoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),   // input type?
        llvm::Type::getInt8Ty(*currentPackage->context),    // output type, which we get pointer to?
        stringLengthWithNull,                                       // size, matches input type?
        nullptr,
        nullptr,
        ""
    );
    currentPackage->currentModule->builder->Insert(memoryPointer);

    // int snprintf ( char * s, size_t n, const char * format, ... );
    ArrayRef<Value *> arguments({
        memoryPointer,
        stringLengthWithNull,
        geti8StrVal(*currentPackage->currentModule->module, "%d", "args", true),
        intValue
    });

    currentPackage->currentModule->builder->CreateCall(snprintfFunction, arguments);
    currentPackage->currentModule->builder->CreateStore(memoryPointer, pointerGEP);

    currentPackage->currentModule->builder->CreateRet(stringMemoryPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}
