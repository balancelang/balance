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
    bmodule->importedFunctions[bfunction->name] = ibfunction;
}

void createImportedClass(BalanceModule *bmodule, BalanceType * btype) {
    BalanceType * ibtype = new BalanceType(bmodule, btype->name);
    // TODO: Figure out a better way to do this.
    ibtype->generics = btype->generics;
    ibtype->internalType = btype->internalType;
    ibtype->constructor = btype->constructor;
    ibtype->isSimpleType = btype->isSimpleType;
    bmodule->importedTypes[btype->toString()] = ibtype;

    // Import class properties
    ibtype->properties = btype->properties;

    // Import each class method
    for (auto const &x : btype->getMethods()) {
        BalanceFunction *bfunction = x.second;
        BalanceFunction *ibfunction = new BalanceFunction(bfunction->name, bfunction->parameters, bfunction->returnType);
        ibtype->methods[bfunction->name] = ibfunction;
        std::string functionNameWithClass = ibtype->toString() + "_" + ibfunction->name;
        ibfunction->function = Function::Create(bfunction->function->getFunctionType(), Function::ExternalLinkage, functionNameWithClass, bmodule->module);
    }

    // Import constructor
    if (btype->internalType != nullptr && !btype->isSimpleType) {
        if (ibtype->constructor != nullptr) {
            // TODO: We will need to differentiate constructors when allowing constructor-overloading
            std::string constructorName = ibtype->toString() + "_constructor";
            FunctionType *constructorType = ibtype->constructor->getFunctionType();
            ibtype->constructor = Function::Create(constructorType, Function::ExternalLinkage, constructorName, bmodule->module);
        } else {
            // TODO: Should this be able to happen? Internal types e.g.?
        }
    } else {
        // TODO: Can this happen?
    }

    // TODO: Does the type as a whole need forward declarations?
}

void createDefaultConstructor(BalanceModule *bmodule, BalanceType * btype) {
    std::string constructorName = btype->toString() + "_constructor";
    vector<Type *> functionParameterTypes;

    // TODO: Constructor should return Type of class?
    BalanceType * returnType = currentPackage->currentModule->getType("None");

    ArrayRef<Type *> parametersReference{btype->getReferencableType()};
    FunctionType *functionType = FunctionType::get(returnType->getInternalType(), parametersReference, false);
    Function *function = Function::Create(functionType, Function::ExternalLinkage, constructorName, bmodule->module);
    btype->constructor = function;

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    llvm::Value *thisValue = args++;
    thisValue->setName("this");

    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, constructorName + "_body", function);
    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    for (auto const &x : btype->properties) {
        BalanceProperty *property = x.second;
        Type *propertyType = property->balanceType->getReferencableType();

        Value *initialValue = nullptr;
        if (propertyType->isIntegerTy()) {
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