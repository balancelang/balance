#include "Array.h"
#include "../Builtins.h"
#include "../Package.h"

#include "../models/BalanceTypeString.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;
extern llvm::Value *accessedValue;
extern std::map<llvm::Value *, BalanceTypeString *> typeLookup;

void createMethod_Array_toString() {
    BalanceParameter * valueParameter = new BalanceParameter(new BalanceTypeString("Array"), "value");
    valueParameter->type = getBuiltinType(new BalanceTypeString("Array"));

    std::string functionName = "toString";
    std::string functionNameWithClass = "Array_" + functionName;

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        getBuiltinType(new BalanceTypeString("Array"))
    });

    FunctionType *functionType = FunctionType::get(getBuiltinType(new BalanceTypeString("String")), parametersReference, false);

    llvm::Function * arrayToStringFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", arrayToStringFunc);

    currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, parameters, new BalanceTypeString("String"));
    currentPackage->currentModule->currentClass->methods[functionName]->function = arrayToStringFunc;
    currentPackage->currentModule->currentClass->methods[functionName]->returnType = getBuiltinType(new BalanceTypeString("String"));

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = arrayToStringFunc->arg_begin();
    llvm::Value * arrayValue = args++;

    BalanceClass * stringClass = currentPackage->builtins->getClass("String");
    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringClass->structType,                                // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringClass->structType),       // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> argumentsReference{stringMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(stringClass->constructor, argumentsReference);
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["stringPointer"]->index, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringMemoryPointer, {pointerZeroValue, pointerIndexValue});
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["length"]->index, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringMemoryPointer, {sizeZeroValue, sizeIndexValue});

    // TODO: Figure out how we detect type at runtime?
    // Add a 'type' number in struct? How do we handle e.g. Dictionary<String, Int> being T?

    currentPackage->currentModule->builder->CreateRet(stringMemoryPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createType__Array() {
    BalanceClass * arrayClass = new BalanceClass("Array");
    currentPackage->currentModule->classes["Array"] = arrayClass;

    arrayClass->properties["memoryPointer"] = new BalanceProperty("memoryPointer", "", 0);
    arrayClass->properties["memoryPointer"]->type = llvm::Type::getInt8PtrTy(*currentPackage->context);
    arrayClass->properties["length"] = new BalanceProperty("length", "", 1, true);
    arrayClass->properties["length"]->type = llvm::Type::getInt32Ty(*currentPackage->context);
    arrayClass->properties["elementSize"] = new BalanceProperty("elementSize", "", 2);
    arrayClass->properties["elementSize"]->type = llvm::Type::getInt32Ty(*currentPackage->context);
    // TODO: Can we store element type instead, and get size from it?

    currentPackage->currentModule->currentClass = arrayClass;
    StructType *structType = StructType::create(*currentPackage->context, "Array");
    ArrayRef<Type *> propertyTypesRef({
        arrayClass->properties["memoryPointer"]->type,
        arrayClass->properties["length"]->type,
        arrayClass->properties["elementSize"]->type
    });
    structType->setBody(propertyTypesRef, false);
    arrayClass->structType = structType;
    arrayClass->hasBody = true;

    createDefaultConstructor(currentPackage->currentModule, arrayClass);

    createMethod_Array_toString();

    currentPackage->currentModule->currentClass = nullptr;
}