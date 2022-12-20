#include "Utilities.h"
#include "BalancePackage.h"
#include "visitors/Visitor.h"
#include "models/BalanceTypeString.h"

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
    BalanceFunction *ibfunction = new BalanceFunction(bfunction->name, bfunction->parameters, bfunction->returnTypeString);
    ibfunction->function = Function::Create(bfunction->function->getFunctionType(), Function::ExternalLinkage, bfunction->name, bmodule->module);
    bmodule->importedFunctions[bfunction->name] = ibfunction;
}

void createImportedClass(BalanceModule *bmodule, BalanceClass *bclass) {
    BalanceClass *ibclass = new BalanceClass(bclass->name, bmodule);
    // TODO: Figure out a better way to do this.
    ibclass->internalType = bclass->internalType;
    ibclass->constructor = bclass->constructor;
    ibclass->isSimpleType = bclass->isSimpleType;
    bmodule->importedClasses[bclass->name->toString()] = ibclass;

    // Import class properties
    ibclass->properties = bclass->properties;

    // Import each class method
    for (auto const &x : bclass->getMethods()) {
        BalanceFunction *bfunction = x.second;
        BalanceFunction *ibfunction = new BalanceFunction(bfunction->name, bfunction->parameters, bfunction->returnTypeString);
        ibclass->methods[bfunction->name] = ibfunction;
        std::string functionNameWithClass = ibclass->name->toString() + "_" + ibfunction->name;
        ibfunction->function = Function::Create(bfunction->function->getFunctionType(), Function::ExternalLinkage, functionNameWithClass, bmodule->module);
    }

    // Import constructor
    if (bclass->internalType != nullptr && !bclass->isSimpleType) {
        if (ibclass->constructor != nullptr) {
            // TODO: We will need to differentiate constructors when allowing constructor-overloading
            std::string constructorName = ibclass->name->toString() + "_constructor";
            FunctionType *constructorType = ibclass->constructor->getFunctionType();
            ibclass->constructor = Function::Create(constructorType, Function::ExternalLinkage, constructorName, bmodule->module);
        } else {
            // TODO: Should this be able to happen? Internal types e.g.?
        }
    } else {
        // TODO: Can this happen?
    }

    // TODO: Does the type as a whole need forward declarations?
}

void createDefaultConstructor(BalanceModule *bmodule, BalanceClass *bclass) {
    std::string constructorName = bclass->name->toString() + "_constructor";
    vector<Type *> functionParameterTypes;

    // TODO: Constructor should return Type of class?
    Type *returnType = getBuiltinType(new BalanceTypeString("None"));

    ArrayRef<Type *> parametersReference{bclass->getReferencableType()};
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
    Function *function = Function::Create(functionType, Function::ExternalLinkage, constructorName, bmodule->module);
    bclass->constructor = function;

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    llvm::Value *thisValue = args++;
    thisValue->setName("this");

    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, constructorName + "_body", function);
    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    for (auto const &x : bclass->properties) {
        BalanceProperty *property = x.second;
        Type *propertyType = property->type;

        Value *initialValue = nullptr;
        if (propertyType->isIntegerTy(1)) {
            initialValue = ConstantInt::get(getBuiltinType(new BalanceTypeString("Bool")), 0, true);
        } else if (propertyType->isIntegerTy(32)) {
            initialValue = ConstantInt::get(getBuiltinType(new BalanceTypeString("Int")), 0, true);
        } else if (propertyType->isFloatingPointTy()) {
            initialValue = ConstantFP::get(getBuiltinType(new BalanceTypeString("Double")), 0.0);
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
            std::cout << "Unhandled initial value for type: " << propertyType << std::endl;
            exit(1);
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
        throw std::runtime_error("Error verifying default constructor for class: " + bclass->name->toString());
    }
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

bool balanceTypeStringsEqual(BalanceTypeString * a, BalanceTypeString * b) {
    if (a->base != b->base) {
        return false;
    }

    if (a->generics.size() != b->generics.size()) {
        return false;
    }

    for (int i = 0; i < a->generics.size(); i++) {
        BalanceTypeString * ai = a->generics[i];
        BalanceTypeString * bi = b->generics[i];

        if (ai->base != bi->base) {
            return false;
        }
        bool subResult = balanceTypeStringsEqual(ai, bi);
        if (!subResult) {
            return false;
        }
    }

    return true;
}