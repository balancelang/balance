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

    BalanceParameter * valueParameter = new BalanceParameter(new BalanceTypeString("Double"), "value");
    valueParameter->type = getBuiltinType(new BalanceTypeString("Double"));

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        getBuiltinType(new BalanceTypeString("Double"))
    });

    FunctionType *functionType = FunctionType::get(getBuiltinType(new BalanceTypeString("String")), parametersReference, false);

    llvm::Function * doubleToStringFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", doubleToStringFunc);

    currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, parameters, new BalanceTypeString("String"));
    currentPackage->currentModule->currentClass->methods[functionName]->function = doubleToStringFunc;
    currentPackage->currentModule->currentClass->methods[functionName]->returnType = getBuiltinType(new BalanceTypeString("String"));

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = doubleToStringFunc->arg_begin();
    llvm::Value * intValue = args++;

    BalanceClass * stringClass = currentPackage->builtins->getClass(new BalanceTypeString("String"));
    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringClass->structType,                                // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringClass->structType),       // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> argumentsReference{stringMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(stringClass->constructor, argumentsReference);
    int pointerIndex = stringClass->properties["stringPointer"]->index;
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, pointerIndex, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringMemoryPointer, {pointerZeroValue, pointerIndexValue});
    int sizeIndex = stringClass->properties["length"]->index;
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, sizeIndex, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringMemoryPointer, {sizeZeroValue, sizeIndexValue});

    // Calculate length of string with int length = snprintf(NULL, 0,"%g",42);
    ArrayRef<Value *> sizeArguments({
        ConstantPointerNull::get(Type::getInt8PtrTy(*currentPackage->context)),
        ConstantInt::get(*currentPackage->context, APInt(32, 0)),
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
        ConstantInt::get(*currentPackage->context, APInt(32, 50)),
        geti8StrVal(*currentPackage->currentModule->module, "%g", "args", true),
        intValue
    });

    currentPackage->currentModule->builder->CreateCall(snprintfFunction, arguments);
    currentPackage->currentModule->builder->CreateStore(memoryPointer, pointerGEP);

    currentPackage->currentModule->builder->CreateRet(stringMemoryPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createType__Double() {
    auto typeString = new BalanceTypeString("Double");
    BalanceClass * bclass = new BalanceClass(typeString);
    bclass->type = getBuiltinType(typeString);

    currentPackage->currentModule->classes["Double"] = bclass;
    currentPackage->currentModule->currentClass = bclass;

    createMethod_Double_toString();

    currentPackage->currentModule->currentClass = nullptr;
}