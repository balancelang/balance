#include "Bool.h"
#include "../Builtins.h"
#include "../Package.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;
extern llvm::Value *accessedValue;

void createMethod_Double_toString() {
    // Create forward declaration of snprintf
    ArrayRef<Type *> snprintfArguments({
        llvm::Type::getInt8PtrTy(*currentPackage->context),
        llvm::IntegerType::getInt32Ty(*currentPackage->context),
        llvm::Type::getInt8PtrTy(*currentPackage->context)
    });
    llvm::FunctionType * snprintfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), snprintfArguments, true);
    FunctionCallee snprintfFunction = currentPackage->currentModule->module->getOrInsertFunction("snprintf", snprintfFunctionType);

    std::string functionName = "toString";
    std::string functionNameWithClass = "Double_" + functionName;

    BalanceParameter * valueParameter = new BalanceParameter("Double", "value");
    valueParameter->type = getBuiltinType("Double");

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        getBuiltinType("Double")
    });

    FunctionType *functionType = FunctionType::get(getBuiltinType("String"), parametersReference, false);

    llvm::Function * doubleToStringFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", doubleToStringFunc);

    currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, parameters, "String");
    currentPackage->currentModule->currentClass->methods[functionName]->function = doubleToStringFunc;
    currentPackage->currentModule->currentClass->methods[functionName]->returnType = getBuiltinType("String");

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = doubleToStringFunc->arg_begin();
    llvm::Value * intValue = args++;

    // TODO: malloc
    BalanceClass * stringClass = currentPackage->builtins->getClass("String");
    AllocaInst *alloca = currentPackage->currentModule->builder->CreateAlloca(stringClass->structType);

    ArrayRef<Value *> argumentsReference{alloca};
    currentPackage->currentModule->builder->CreateCall(stringClass->constructor, argumentsReference);
    int pointerIndex = stringClass->properties["stringPointer"]->index;
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, pointerIndex, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, alloca, {pointerZeroValue, pointerIndexValue});
    int sizeIndex = stringClass->properties["length"]->index;
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, sizeIndex, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, alloca, {sizeZeroValue, sizeIndexValue});

    // Calculate length of string with int length = snprintf(NULL, 0,"%d",42);
    ArrayRef<Value *> sizeArguments({
        ConstantPointerNull::get(Type::getInt8PtrTy(*currentPackage->context)),
        ConstantInt::get(*currentPackage->context, APInt(32, 0)),
        geti8StrVal(*currentPackage->currentModule->module, "%g", "args"),
        intValue
    });
    Value * stringLength = currentPackage->currentModule->builder->CreateCall(snprintfFunction, sizeArguments);
    currentPackage->currentModule->builder->CreateStore(stringLength, sizeGEP);

    ArrayType * arrayType = llvm::ArrayType::get(llvm::Type::getInt8Ty(*currentPackage->context), 50);
    // TODO: malloc
    Value * arrayAlloca = currentPackage->currentModule->builder->CreateAlloca(arrayType);
    currentPackage->currentModule->builder->CreateStore(stringLength, sizeGEP);

    Value * bitcastedAlloca = currentPackage->currentModule->builder->CreateBitCast(arrayAlloca, llvm::Type::getInt8PtrTy(*currentPackage->context));
    currentPackage->currentModule->builder->CreateStore(bitcastedAlloca, pointerGEP);

    Value * gepValue = currentPackage->currentModule->builder->CreateLoad(pointerGEP);

    // int snprintf ( char * s, size_t n, const char * format, ... );
    ArrayRef<Value *> arguments({
        gepValue,
        ConstantInt::get(*currentPackage->context, APInt(32, 50)),
        geti8StrVal(*currentPackage->currentModule->module, "%g", "args"),
        intValue
    });

    currentPackage->currentModule->builder->CreateCall(snprintfFunction, arguments);
    currentPackage->currentModule->builder->CreateRet(alloca);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createType__Double() {
    BalanceClass * bclass = new BalanceClass("Double");
    currentPackage->currentModule->classes["Double"] = bclass;
    currentPackage->currentModule->currentClass = bclass;

    createMethod_Double_toString();

    currentPackage->currentModule->currentClass = nullptr;
}