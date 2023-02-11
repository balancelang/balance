#include "Utilities.h"
#include "BalancePackage.h"
#include "visitors/Visitor.h"
#include "models/BalanceType.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>

extern BalancePackage *currentPackage;

bool fileExist(std::string fileName) {
    std::ifstream infile(fileName);
    return infile.good();
}

void createImportedFunction(BalanceModule *bmodule, BalanceFunction *bfunction) {
    BalanceFunction *ibfunction = new BalanceFunction(bfunction->name, bfunction->parameters, bfunction->returnType);
    ibfunction->function = Function::Create(bfunction->function->getFunctionType(), Function::ExternalLinkage, bfunction->name, bmodule->module);
    bmodule->importedFunctions.push_back(ibfunction);
}

BalanceType * createImportedClass(BalanceModule *bmodule, BalanceType * btype) {
    // Check if we already imported this type
    for (BalanceType * ibtype : bmodule->types) {
        if (ibtype->equalTo(btype)) {
            return ibtype;
        }
    }

    BalanceType * ibtype = new BalanceType(bmodule, btype->name);
    // TODO: Figure out a better way to do this.

    for (BalanceType * generic : btype->generics) {
        ibtype->generics.push_back(createImportedClass(bmodule, generic));
    }

    ibtype->internalType = btype->internalType;

    // TODO: Do we need to import this as well?
    ibtype->initializer = btype->initializer;
    ibtype->isSimpleType = btype->isSimpleType;
    ibtype->hasBody = btype->hasBody;
    bmodule->addType(ibtype);

    // Import class properties
    for (auto const &x : btype->properties) {
        BalanceProperty * ibproperty = new BalanceProperty(x.second->name, createImportedClass(bmodule, x.second->balanceType), x.second->isPublic);
        ibproperty->index = x.second->index;
        ibtype->properties[x.first] = ibproperty;
    }

    // Import each class method
    for (BalanceFunction *bfunction : btype->getMethods()) {
        BalanceType * returnType = createImportedClass(bmodule, bfunction->returnType);
        // TODO: Import parameters?
        BalanceFunction *ibfunction = new BalanceFunction(bfunction->name, bfunction->parameters, returnType);
        ibtype->methods[bfunction->name] = ibfunction;
        std::string functionNameWithClass = ibtype->toString() + "_" + ibfunction->name;
        ibfunction->function = Function::Create(bfunction->function->getFunctionType(), Function::ExternalLinkage, functionNameWithClass, bmodule->module);
    }

    // Import default constructor
    if (btype->internalType != nullptr && !btype->isSimpleType) {
        if (ibtype->initializer != nullptr) {
            // TODO: We will need to differentiate constructors when allowing constructor-overloading
            std::string constructorName = ibtype->toString() + "_constructor";
            FunctionType *constructorType = ibtype->initializer->getFunctionType();
            ibtype->initializer = Function::Create(constructorType, Function::ExternalLinkage, constructorName, bmodule->module);
        } else {
            // TODO: Should this be able to happen? Internal types e.g.?
        }
    } else {
        // TODO: Can this happen?
    }

    // Import additional constructors
    for (BalanceFunction * constructor : btype->constructors) {
        std::string constructorName = btype->name + "_" + constructor->name;
        FunctionType *constructorType = constructor->function->getFunctionType();

        BalanceFunction *ibfunction = new BalanceFunction(constructor->name, constructor->parameters, constructor->returnType);
        ibtype->constructors.push_back(ibfunction);
        ibfunction->function = Function::Create(constructorType, Function::ExternalLinkage, constructorName, bmodule->module);
    }

    // TODO: Does the type as a whole need forward declarations?
    return ibtype;
}

void createDefaultConstructor(BalanceModule *bmodule, BalanceType * btype) {
    std::string constructorName = btype->toString() + "_constructor";
    vector<Type *> functionParameterTypes;

    // TODO: Constructor should return Type of class?
    BalanceType * returnType = currentPackage->currentModule->getType("None");

    ArrayRef<Type *> parametersReference{btype->getReferencableType()};
    FunctionType *functionType = FunctionType::get(returnType->getInternalType(), parametersReference, false);
    Function *function = Function::Create(functionType, Function::ExternalLinkage, constructorName, bmodule->module);
    btype->initializer = function;

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    llvm::Value *thisValue = args++;
    thisValue->setName("this");

    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, constructorName + "_body", function);
    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    for (BalanceProperty *property : btype->getProperties()) {
        Type *propertyType = property->balanceType->getReferencableType();

        Value *initialValue = nullptr;
        if (property->name == "typeId") {
            initialValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, btype->typeIndex, true));
        } else if (propertyType->isIntegerTy()) {
            int width = propertyType->getIntegerBitWidth();
            initialValue = ConstantInt::get(*currentPackage->context, llvm::APInt(width, 0, true));
        } else if (propertyType->isFloatingPointTy()) {
            initialValue = ConstantFP::get(Type::getDoubleTy(*currentPackage->context), 0.0);
        } else if (PointerType *PT = dyn_cast<PointerType>(propertyType)) {
            if (propertyType->getPointerElementType()->isIntegerTy(32)) {
                initialValue = ConstantPointerNull::get(Type::getInt32PtrTy(*currentPackage->context));
            } else if (propertyType->getPointerElementType()->isIntegerTy(8)) {
                // Only used for builtins (e.g. File::filePointer) // TODO: Maybe removed and instead use i32*
                initialValue = ConstantPointerNull::get(Type::getInt8PtrTy(*currentPackage->context));
            } else if (propertyType->getPointerElementType()->isStructTy()) {
                // TODO: Does it suffice if we do this for all pointer types? (e.g remove the two above)
                initialValue = ConstantPointerNull::get(PT);
            } else {
                initialValue = ConstantPointerNull::get(PT);
            }
        }

        // TODO: Handle String and nullable types
        if (initialValue == nullptr) {
            throw std::runtime_error("Unhandled initial value for type: " + property->balanceType->toString());
        }

        int intIndex = property->index;
        auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue, {zero, index});
        currentPackage->currentModule->builder->CreateStore(initialValue, ptr);
    }

    // Return void
    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*function, &llvm::errs());
    if (hasError) {
        throw std::runtime_error("Error verifying default constructor for class: " + btype->toString());
    }
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

Constant *geti8StrVal(Module &M, char const *str, Twine const &name, bool addNull) {
    Constant *strConstant = ConstantDataArray::getString(M.getContext(), str, addNull);
    auto *GVStr = new GlobalVariable(M, strConstant->getType(), true, GlobalValue::InternalLinkage, strConstant, name);
    Constant *zero = Constant::getNullValue(IntegerType::getInt32Ty(M.getContext()));
    Constant *indices[] = {zero, zero};
    Constant *strVal = ConstantExpr::getGetElementPtr(strConstant->getType(), GVStr, indices, true);
    return strVal;
}

void createDefaultToStringMethod(BalanceType * btype) {
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    // TODO: This function must be able to handle cycles, so it doesn't visit something it has already visited again
    // Just print: A(b: B(a: [cycle]))
    std::string functionName = "toString";
    std::string functionNameWithClass = btype->toString() + "_" + functionName;

    // Create BalanceFunction
    BalanceParameter * valueParameter = new BalanceParameter(btype, "value");
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };
    BalanceFunction * bfunction = new BalanceFunction(functionName, parameters, stringType);
    btype->addMethod(functionName, bfunction);

    // Create llvm function
    ArrayRef<Type *> parametersReference({
        btype->getReferencableType()
    });

    FunctionType *functionType = FunctionType::get(stringType->getReferencableType(), parametersReference, false);
    llvm::Function * btypeDefaultToStringFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", btypeDefaultToStringFunc);
    bfunction->function = btypeDefaultToStringFunc;

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = btypeDefaultToStringFunc->arg_begin();
    llvm::Value *btypeStructPointer = args++;

    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringType->getInternalType(),                          // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringType->getInternalType()), // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> argumentsReference{stringMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(stringType->getInitializer(), argumentsReference);
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["stringPointer"]->index, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), stringMemoryPointer, {pointerZeroValue, pointerIndexValue});
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringType->properties["length"]->index, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), stringMemoryPointer, {sizeZeroValue, sizeIndexValue});

    BasicBlock *thenBlock = BasicBlock::Create(*currentPackage->context, "then", btypeDefaultToStringFunc);
    BasicBlock *elseBlock = BasicBlock::Create(*currentPackage->context, "else", btypeDefaultToStringFunc);

    // check if null
    Value * nullValue = ConstantPointerNull::get((PointerType *) btype->getReferencableType());
    Value * expression = currentPackage->currentModule->builder->CreateICmpEQ(nullValue, btypeStructPointer);

    currentPackage->currentModule->builder->CreateCondBr(expression, thenBlock, elseBlock);
    currentPackage->currentModule->builder->SetInsertPoint(thenBlock);

    // if value is not null
    Value * nullStringValue = ConstantPointerNull::get((PointerType *) stringType->getReferencableType());
    currentPackage->currentModule->builder->CreateRet(nullStringValue);

    currentPackage->currentModule->builder->SetInsertPoint(elseBlock);

    Value * arrayValueFalse = geti8StrVal(*currentPackage->currentModule->module, btype->toString().c_str(), btype->toString().c_str(), true);
    currentPackage->currentModule->builder->CreateStore(arrayValueFalse, pointerGEP);

    currentPackage->currentModule->builder->CreateRet(stringMemoryPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}