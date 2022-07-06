#include "Builtins.h"
#include "Package.h"
#include "builtins/File.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createFunction__print()
{
    // Create forward declaration of printf
    llvm::FunctionType * printfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), true);
    FunctionCallee printfFunction = currentPackage->currentModule->module->getOrInsertFunction("printf", printfFunctionType);

    // TODO: One day we might change this to "Any" and then do toString on it
    BalanceParameter * contentParameter = new BalanceParameter("String", "content");
    contentParameter->type = getBuiltinType("String");

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        contentParameter
    };
    BalanceFunction * bfunction = new BalanceFunction("print", parameters, "None");
    currentPackage->currentModule->functions["print"] = bfunction;

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        contentParameter->type
    });

    Type * returnType = llvm::Type::getVoidTy(*currentPackage->context);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * printFunc = Function::Create(functionType, Function::ExternalLinkage, "print", currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, "print_body", printFunc);

    bfunction->function = printFunc;
    bfunction->returnType = getBuiltinType("None");

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = printFunc->arg_begin();
    llvm::Value *stringStructPointer = args++;

    BalanceClass * stringClass = currentPackage->builtins->getClass("String");
    Value * zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * stringPointerIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["stringPointer"]->index, true));
    Value * stringPointerValue = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringStructPointer, {zero, stringPointerIndex});
    Value * stringValue = currentPackage->currentModule->builder->CreateLoad(stringPointerValue);

    // CreateCall to printf with stringPointer as argument.
    ArrayRef<Value *> arguments({
        stringValue,
    });
    currentPackage->currentModule->builder->CreateCall(printfFunction, arguments);

    // Create newline TODO: Make this a parameter?
    ArrayRef<Value *> newlineArguments({
        geti8StrVal(*currentPackage->currentModule->module, "\n", "newline"),
    });
    currentPackage->currentModule->builder->CreateCall(printfFunction, newlineArguments);

    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*bfunction->function, &llvm::errs());
    if (hasError) {
        // TODO: Throw error
        std::cout << "Error verifying function: " << bfunction->name << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        // exit(1);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createFunction__printBoolean() {
    BalanceParameter * valueParameter = new BalanceParameter("Bool", "value");
    valueParameter->type = getBuiltinType("Bool");

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };
    BalanceFunction * bfunction = new BalanceFunction("printBoolean", parameters, "None");
    currentPackage->currentModule->functions["printBoolean"] = bfunction;

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        valueParameter->type
    });

    Type * returnType = llvm::Type::getVoidTy(*currentPackage->context);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * printBooleanFunc = Function::Create(functionType, Function::ExternalLinkage, "printBoolean", currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, "printBoolean_body", printBooleanFunc);

    bfunction->function = printBooleanFunc;
    bfunction->returnType = getBuiltinType("None");

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = printBooleanFunc->arg_begin();
    llvm::Value *boolValue = args++;

    FunctionCallee printFunc = currentPackage->currentModule->getFunction("print")->function;
    // BEFORE

    BalanceClass * bclass = currentPackage->builtins->getClass("String");
    AllocaInst *alloca = currentPackage->currentModule->builder->CreateAlloca(bclass->structType);
    ArrayRef<Value *> argumentsReference{alloca};
    currentPackage->currentModule->builder->CreateCall(bclass->constructor, argumentsReference);
    int pointerIndex = bclass->properties["stringPointer"]->index;
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, pointerIndex, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(bclass->structType, alloca, {pointerZeroValue, pointerIndexValue});
    int sizeIndex = bclass->properties["stringSize"]->index;
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, sizeIndex, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(bclass->structType, alloca, {sizeZeroValue, sizeIndexValue});

    ArrayRef<Value *> arguments({
        alloca,
    });

    // Create if-statement to print 'true' or 'false'
    BasicBlock *thenBlock = BasicBlock::Create(*currentPackage->context, "then", printBooleanFunc);
    BasicBlock *elseBlock = BasicBlock::Create(*currentPackage->context, "else", printBooleanFunc);
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->context, "ifcont", printBooleanFunc);

    Value * zero = ConstantInt::get(*currentPackage->context, llvm::APInt(1, 0, true));
    Value * expression = currentPackage->currentModule->builder->CreateICmpEQ(zero, boolValue);

    currentPackage->currentModule->builder->CreateCondBr(expression, thenBlock, elseBlock);

    currentPackage->currentModule->builder->SetInsertPoint(thenBlock);
    // if value == 0 (false)                                                                        // 5 = 'false'
    Value * sizeValueFalse = (Value *) ConstantInt::get(IntegerType::getInt32Ty(*currentPackage->context), 5, true);
    currentPackage->currentModule->builder->CreateStore(sizeValueFalse, sizeGEP);
    Value * arrayValueFalse = geti8StrVal(*currentPackage->currentModule->module, "false", "false");
    // TODO: Create arrayValue as the i8* representing 'false'
    currentPackage->currentModule->builder->CreateStore(arrayValueFalse, pointerGEP);
    currentPackage->currentModule->builder->CreateCall(printFunc, arguments);

    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    thenBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(elseBlock);

    // if value != 0 (true)                                                                         // 4 = 'true'
    Value * sizeValueTrue = (Value *) ConstantInt::get(IntegerType::getInt32Ty(*currentPackage->context), 4, true);
    currentPackage->currentModule->builder->CreateStore(sizeValueTrue, sizeGEP);

    // TODO: Create arrayValue as the i8* representing 'true'
    Value * arrayValueTrue = geti8StrVal(*currentPackage->currentModule->module, "true", "true");
    currentPackage->currentModule->builder->CreateStore(arrayValueTrue, pointerGEP);
    currentPackage->currentModule->builder->CreateCall(printFunc, arguments);

    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    elseBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*bfunction->function, &llvm::errs());
    if (hasError) {
        // TODO: Throw error
        std::cout << "Error verifying function: " << bfunction->name << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        // exit(1);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createFunction__printDouble() {
    // Create forward declaration of snprintf
    ArrayRef<Type *> snprintfArguments({
        llvm::Type::getInt8PtrTy(*currentPackage->context),
        llvm::IntegerType::getInt32Ty(*currentPackage->context),
        llvm::Type::getInt8PtrTy(*currentPackage->context)
    });
    llvm::FunctionType * snprintfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), snprintfArguments, true);
    FunctionCallee snprintfFunction = currentPackage->currentModule->module->getOrInsertFunction("snprintf", snprintfFunctionType);

    BalanceParameter * valueParameter = new BalanceParameter("Double", "value");
    valueParameter->type = getBuiltinType("Double");

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };
    BalanceFunction * bfunction = new BalanceFunction("printDouble", parameters, "None");
    currentPackage->currentModule->functions["printDouble"] = bfunction;

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        valueParameter->type
    });

    Type * returnType = llvm::Type::getVoidTy(*currentPackage->context);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * printDoubleFunc = Function::Create(functionType, Function::ExternalLinkage, "printDouble", currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, "printDouble_body", printDoubleFunc);

    bfunction->function = printDoubleFunc;
    bfunction->returnType = getBuiltinType("None");

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = printDoubleFunc->arg_begin();
    llvm::Value * intValue = args++;

    FunctionCallee printFunc = currentPackage->currentModule->getFunction("print")->function;
    // BEFORE

    BalanceClass * bclass = currentPackage->builtins->getClass("String");
    AllocaInst *alloca = currentPackage->currentModule->builder->CreateAlloca(bclass->structType);
    ArrayRef<Value *> argumentsReference{alloca};
    currentPackage->currentModule->builder->CreateCall(bclass->constructor, argumentsReference);
    int pointerIndex = bclass->properties["stringPointer"]->index;
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, pointerIndex, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(bclass->structType, alloca, {pointerZeroValue, pointerIndexValue});
    int sizeIndex = bclass->properties["stringSize"]->index;
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, sizeIndex, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(bclass->structType, alloca, {sizeZeroValue, sizeIndexValue});

    // Calculate length of string with int length = snprintf(NULL, 0,"%d",42);
    ArrayRef<Value *> sizeArguments({
        ConstantPointerNull::get(Type::getInt8PtrTy(*currentPackage->context)),
        ConstantInt::get(*currentPackage->context, APInt(32, 0)),
        geti8StrVal(*currentPackage->currentModule->module, "%lf", "args"),
        intValue
    });
    Value * stringLength = currentPackage->currentModule->builder->CreateCall(snprintfFunction, sizeArguments);
    currentPackage->currentModule->builder->CreateStore(stringLength, sizeGEP);

    ArrayType * arrayType = llvm::ArrayType::get(llvm::Type::getInt8Ty(*currentPackage->context), 50);
    Value * arrayAlloca = currentPackage->currentModule->builder->CreateAlloca(arrayType);
    currentPackage->currentModule->builder->CreateStore(arrayAlloca, pointerGEP);
    currentPackage->currentModule->builder->CreateStore(stringLength, sizeGEP);

    Value * gepValue = currentPackage->currentModule->builder->CreateLoad(pointerGEP);

    // int snprintf ( char * s, size_t n, const char * format, ... );
    ArrayRef<Value *> arguments({
        gepValue, //pointerGEP,
        ConstantInt::get(*currentPackage->context, APInt(32, 50)),
        geti8StrVal(*currentPackage->currentModule->module, "%lf", "args"),
        intValue
    });

    currentPackage->currentModule->builder->CreateCall(snprintfFunction, arguments);

    // Now call print with the String
    ArrayRef<Value *> printArguments({
        alloca,
    });
    currentPackage->currentModule->builder->CreateCall(printFunc, printArguments);

    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*bfunction->function, &llvm::errs());
    if (hasError) {
        // TODO: Throw error
        std::cout << "Error verifying function: " << bfunction->name << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        // exit(1);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createFunction__printInt() {
    // Create forward declaration of snprintf
    ArrayRef<Type *> snprintfArguments({
        llvm::Type::getInt8PtrTy(*currentPackage->context),
        llvm::IntegerType::getInt32Ty(*currentPackage->context),
        llvm::Type::getInt8PtrTy(*currentPackage->context)
    });
    llvm::FunctionType * snprintfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), snprintfArguments, true);
    FunctionCallee snprintfFunction = currentPackage->currentModule->module->getOrInsertFunction("snprintf", snprintfFunctionType);

    BalanceParameter * valueParameter = new BalanceParameter("Int", "value");
    valueParameter->type = getBuiltinType("Int");

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };
    BalanceFunction * bfunction = new BalanceFunction("printInt", parameters, "None");
    currentPackage->currentModule->functions["printInt"] = bfunction;

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        valueParameter->type
    });

    Type * returnType = llvm::Type::getVoidTy(*currentPackage->context);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * printIntFunc = Function::Create(functionType, Function::ExternalLinkage, "printInt", currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, "printInt_body", printIntFunc);

    bfunction->function = printIntFunc;
    bfunction->returnType = getBuiltinType("None");

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = printIntFunc->arg_begin();
    llvm::Value * intValue = args++;

    FunctionCallee printFunc = currentPackage->currentModule->getFunction("print")->function;
    // BEFORE

    BalanceClass * bclass = currentPackage->builtins->getClass("String");
    AllocaInst *alloca = currentPackage->currentModule->builder->CreateAlloca(bclass->structType);
    ArrayRef<Value *> argumentsReference{alloca};
    currentPackage->currentModule->builder->CreateCall(bclass->constructor, argumentsReference);
    int pointerIndex = bclass->properties["stringPointer"]->index;
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, pointerIndex, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(bclass->structType, alloca, {pointerZeroValue, pointerIndexValue});
    int sizeIndex = bclass->properties["stringSize"]->index;
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, sizeIndex, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(bclass->structType, alloca, {sizeZeroValue, sizeIndexValue});

    // Calculate length of string with int length = snprintf(NULL, 0,"%d",42);
    ArrayRef<Value *> sizeArguments({
        ConstantPointerNull::get(Type::getInt8PtrTy(*currentPackage->context)),
        ConstantInt::get(*currentPackage->context, APInt(32, 0)),
        geti8StrVal(*currentPackage->currentModule->module, "%d", "args"),
        intValue
    });
    Value * stringLength = currentPackage->currentModule->builder->CreateCall(snprintfFunction, sizeArguments);
    currentPackage->currentModule->builder->CreateStore(stringLength, sizeGEP);

    ArrayType * arrayType = llvm::ArrayType::get(llvm::Type::getInt8Ty(*currentPackage->context), 50);
    Value * arrayAlloca = currentPackage->currentModule->builder->CreateAlloca(arrayType);
    currentPackage->currentModule->builder->CreateStore(arrayAlloca, pointerGEP);
    currentPackage->currentModule->builder->CreateStore(stringLength, sizeGEP);

    Value * gepValue = currentPackage->currentModule->builder->CreateLoad(pointerGEP);

    // int snprintf ( char * s, size_t n, const char * format, ... );
    ArrayRef<Value *> arguments({
        gepValue, //pointerGEP,
        ConstantInt::get(*currentPackage->context, APInt(32, 50)),
        geti8StrVal(*currentPackage->currentModule->module, "%d", "args"),
        intValue
    });

    currentPackage->currentModule->builder->CreateCall(snprintfFunction, arguments);

    // Now call print with the String
    ArrayRef<Value *> printArguments({
        alloca,
    });
    currentPackage->currentModule->builder->CreateCall(printFunc, printArguments);

    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*bfunction->function, &llvm::errs());
    if (hasError) {
        // TODO: Throw error
        std::cout << "Error verifying function: " << bfunction->name << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        // exit(1);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createFunction__open()
{
    // Create forward declaration of fopen
    llvm::FunctionType * functionType = llvm::FunctionType::get(llvm::PointerType::get(llvm::Type::getInt32Ty(*currentPackage->context), 0), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), false);
    currentPackage->currentModule->module->getOrInsertFunction("fopen", functionType);
}

void createFunctions() {
    createFunction__print();
    createFunction__printBoolean();
    createFunction__printInt();
    createFunction__printDouble();
    createFunction__open();
}

void createTypes() {
    createType__String();
    createType__File();
}

void createBuiltins() {
    BalanceModule * bmodule = new BalanceModule("builtins", false);
    currentPackage->builtins = bmodule;
    currentPackage->currentModule = bmodule;

    // TODO: Check if this works creating types multiple times with multiple modules
    createTypes();
    createFunctions();

    bmodule->builder->CreateRet(ConstantInt::get(*currentPackage->context, APInt(32, 0)));
}