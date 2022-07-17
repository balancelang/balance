#include "Array.h"
#include "../Builtins.h"
#include "../Package.h"

#include "../models/BalanceTypeString.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;
extern llvm::Value *accessedValue;
extern std::map<llvm::Value *, BalanceTypeString *> typeLookup;

void createMethod_Array_toString(BalanceClass * bclass) {
    BalanceParameter * valueParameter = new BalanceParameter(bclass->name, "value");
    valueParameter->type = bclass->structType->getPointerTo();

    std::string functionName = "toString";
    std::string functionNameWithClass = bclass->name->toString() + "_" + functionName;

    // Create BalanceFunction
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };

    // Create llvm::Function
    ArrayRef<Type *> parametersReference({
        bclass->structType->getPointerTo()
    });

    FunctionType *functionType = FunctionType::get(getBuiltinType(new BalanceTypeString("String")), parametersReference, false);

    llvm::Function * arrayToStringFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->builtins->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", arrayToStringFunc);

    bclass->methods[functionName] = new BalanceFunction(functionName, parameters, new BalanceTypeString("String"));
    bclass->methods[functionName]->function = arrayToStringFunc;
    bclass->methods[functionName]->returnType = getBuiltinType(new BalanceTypeString("String"));

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->builtins->builder->GetInsertBlock();
    currentPackage->builtins->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = arrayToStringFunc->arg_begin();
    llvm::Value * arrayValue = args++;

    BalanceClass * stringClass = currentPackage->builtins->getClassFromBaseName("String");
    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->builtins->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringClass->structType,                                // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringClass->structType),       // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->builtins->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> argumentsReference{stringMemoryPointer};
    currentPackage->builtins->builder->CreateCall(stringClass->constructor, argumentsReference);
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["stringPointer"]->index, true));
    auto pointerGEP = currentPackage->builtins->builder->CreateGEP(stringClass->structType, stringMemoryPointer, {pointerZeroValue, pointerIndexValue});
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["length"]->index, true));
    auto sizeGEP = currentPackage->builtins->builder->CreateGEP(stringClass->structType, stringMemoryPointer, {sizeZeroValue, sizeIndexValue});

    // TODO: Figure out how we detect type at runtime?
    // Add a 'type' number in struct? How do we handle e.g. Dictionary<String, Int> being T?

    currentPackage->builtins->builder->CreateRet(stringMemoryPointer);
    currentPackage->builtins->builder->SetInsertPoint(resumeBlock);
}

void createType__Array(BalanceTypeString * typeString) {
    BalanceClass * arrayClass = new BalanceClass(typeString);
    currentPackage->builtins->classes[typeString->toString()] = arrayClass;

    arrayClass->properties["memoryPointer"] = new BalanceProperty("memoryPointer", "", 0);
    arrayClass->properties["memoryPointer"]->type = typeString->generics[0]->type->getPointerTo();
    arrayClass->properties["length"] = new BalanceProperty("length", "", 1, true);
    arrayClass->properties["length"]->type = llvm::Type::getInt32Ty(*currentPackage->context);

    StructType *structType = StructType::create(*currentPackage->context, typeString->toString());
    ArrayRef<Type *> propertyTypesRef({
        arrayClass->properties["memoryPointer"]->type,
        arrayClass->properties["length"]->type,
    });
    structType->setBody(propertyTypesRef, false);
    arrayClass->structType = structType;
    arrayClass->hasBody = true;

    // TODO: this doesn't seem like a good way to do it
    typeString->type = arrayClass->structType->getPointerTo();

    createDefaultConstructor(currentPackage->currentModule, arrayClass);

    // createMethod_Array_toString(arrayClass);
}