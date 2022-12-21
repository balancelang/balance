#include "Builtins.h"
#include "BalancePackage.h"
#include "builtins/File.h"
#include "builtins/Int.h"
#include "builtins/String.h"
#include "builtins/Bool.h"
#include "builtins/Double.h"
#include "builtins/Array.h"

#include "models/BalanceTypeString.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void createFunction__print()
{
    // Create forward declaration of printf
    llvm::FunctionType * printfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), true);
    FunctionCallee printfFunction = currentPackage->currentModule->module->getOrInsertFunction("printf", printfFunctionType);

    // TODO: One day we might change this to "Any" and then do toString on it
    BalanceParameter * contentParameter = new BalanceParameter(new BalanceTypeString("String"), "content");
    contentParameter->type = getBuiltinType(new BalanceTypeString("String"));

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        contentParameter
    };
    BalanceFunction * bfunction = new BalanceFunction("print", parameters, new BalanceTypeString("None"));
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
    bfunction->returnType = getBuiltinType(new BalanceTypeString("None"));

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = printFunc->arg_begin();
    llvm::Value *stringStructPointer = args++;

    BalanceType * stringType = currentPackage->builtins->getType(new BalanceTypeString("String"));
    Value * zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * stringPointerIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["stringPointer"]->index, true));
    Value * stringPointerValue = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), stringStructPointer, {zero, stringPointerIndex});
    Value * stringValue = currentPackage->currentModule->builder->CreateLoad(stringPointerValue);

    // CreateCall to printf with stringPointer as argument.
    ArrayRef<Value *> arguments({
        stringValue,
    });
    currentPackage->currentModule->builder->CreateCall(printfFunction, arguments);

    // Create newline TODO: Make this a parameter?
    ArrayRef<Value *> newlineArguments({
        geti8StrVal(*currentPackage->currentModule->module, "\n", "newline", true),
    });
    currentPackage->currentModule->builder->CreateCall(printfFunction, newlineArguments);

    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*bfunction->function, &llvm::errs());
    if (hasError) {
        throw std::runtime_error("Error verifying function: " + bfunction->name);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createFunction__open()
{
    BalanceParameter * pathParameter = new BalanceParameter(new BalanceTypeString("String"), "path");
    pathParameter->type = getBuiltinType(new BalanceTypeString("String"));

    BalanceParameter * modeParameter = new BalanceParameter(new BalanceTypeString("String"), "mode");
    modeParameter->type = getBuiltinType(new BalanceTypeString("String"));

    std::vector<BalanceParameter *> parameters = {
        pathParameter,
        modeParameter
    };
    BalanceFunction * bfunction = new BalanceFunction("open", parameters, new BalanceTypeString("File"));
    BalanceType * fileType = currentPackage->builtins->getType(new BalanceTypeString("File"));
    bfunction->returnType = fileType->getReferencableType();

    // Build llvm function and assign to bfunction

    // Create forward declaration of fopen
    llvm::FunctionType * fopenfunctionType = llvm::FunctionType::get(llvm::PointerType::get(llvm::Type::getInt32Ty(*currentPackage->context), 0), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), false);
    currentPackage->currentModule->module->getOrInsertFunction("fopen", fopenfunctionType);
    Function *fopenFunc = currentPackage->builtins->module->getFunction("fopen");

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        pathParameter->type,
        modeParameter->type
    });
    FunctionType *functionType = FunctionType::get(bfunction->returnType, parametersReference, false);
    llvm::Function * openFunction = Function::Create(functionType, Function::ExternalLinkage, bfunction->name, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, bfunction->name + "_body", openFunction);

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    BalanceType *stringType = currentPackage->currentModule->getType(new BalanceTypeString("String"));

    Value *zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto functionArgs = openFunction->arg_begin();

    Value * pathStringValue = functionArgs++;
    Value * modeStringValue = functionArgs++;

    Value *stringPointerIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["stringPointer"]->index, true));
    Value *pathPointerValue = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), pathStringValue, {zero, stringPointerIndex});
    Value *pathValue = currentPackage->currentModule->builder->CreateLoad(pathPointerValue);
    Value *modePointerValue = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), modeStringValue, {zero, stringPointerIndex});
    Value *modeValue = currentPackage->currentModule->builder->CreateLoad(modePointerValue);

    auto args = ArrayRef<Value *>({pathValue, modeValue});
    Value *filePointer = currentPackage->currentModule->builder->CreateCall(fopenFunc, args);

    // Create File struct which holds this and return pointer to the struct
    BalanceClass *fileClass = currentPackage->builtins->classes["File"];

    auto structSize = ConstantExpr::getSizeOf(fileClass->getInternalType());
    auto pointer = llvm::CallInst::CreateMalloc(currentPackage->currentModule->builder->GetInsertBlock(), fileClass->getReferencableType(), fileClass->getInternalType(), structSize, nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(pointer);

    ArrayRef<Value *> argumentsReference{pointer};
    currentPackage->currentModule->builder->CreateCall(fileClass->constructor, argumentsReference); // TODO: should it have a constructor?

    // Get reference to 0th property (filePointer) and assign
    int intIndex = fileClass->properties["filePointer"]->index;
    auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));

    auto ptr = currentPackage->currentModule->builder->CreateGEP(fileClass->getInternalType(), pointer, {zero, index});
    currentPackage->currentModule->builder->CreateStore(filePointer, ptr);

    currentPackage->currentModule->builder->CreateRet(pointer);

    // Assign the llvm function to the bfunction
    bfunction->function = openFunction;
    currentPackage->currentModule->functions["open"] = bfunction;

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createType__FatPointer() {
    // TODO: Make sure you can't instantiate this from balance
    auto typeString = new BalanceTypeString("FatPointer");
    BalanceClass * bclass = new BalanceClass(typeString, currentPackage->currentModule);

    currentPackage->currentModule->classes["FatPointer"] = bclass;
    bclass->properties["thisPointer"] = new BalanceProperty("thisPointer", nullptr, 0, false);
    bclass->properties["thisPointer"]->type = llvm::Type::getInt64PtrTy(*currentPackage->context);
    bclass->properties["vtablePointer"] = new BalanceProperty("vtablePointer", nullptr, 1, true);
    bclass->properties["vtablePointer"]->type = llvm::Type::getInt64PtrTy(*currentPackage->context);

    currentPackage->currentModule->currentClass = bclass;
    StructType *structType = StructType::create(*currentPackage->context, "FatPointer");
    ArrayRef<Type *> propertyTypesRef({
        bclass->properties["thisPointer"]->type,
        bclass->properties["vtablePointer"]->type
    });
    structType->setBody(propertyTypesRef, false);
    bclass->internalType = structType;
    bclass->hasBody = true;

    // TODO: Might not be needed
    createDefaultConstructor(currentPackage->currentModule, bclass);

    currentPackage->currentModule->currentClass = nullptr;
}

void createFunctions() {
    createFunction__print();
    createFunction__open();
}

void createTypes() {
    createType__FatPointer();

    createType__String();
    createType__Int();
    createType__Bool();
    createType__Double();
    createType__File();

    // Arrays are lazily created with their generic types
}

void createBuiltins() {
    BalanceModule * bmodule = new BalanceModule("builtins", false);
    bmodule->initializeModule();
    currentPackage->builtins = bmodule;
    currentPackage->currentModule = bmodule;

    createTypes();
    createFunctions();

    bmodule->builder->CreateRet(ConstantInt::get(*currentPackage->context, APInt(32, 0)));
}