#include "Utilities.h"
#include "Package.h"
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
    BalanceImportedFunction *ibfunction = new BalanceImportedFunction(bmodule, bfunction);
    bmodule->importedFunctions[bfunction->name] = ibfunction;
}

void createImportedClass(BalanceModule *bmodule, BalanceClass *bclass) {
    BalanceImportedClass *ibclass = new BalanceImportedClass(bmodule, bclass);
    bmodule->importedClasses[bclass->name] = ibclass;

    // Create BalanceImportedFunction for each class method
    for (auto const &x : bclass->methods) {
        BalanceFunction *bfunction = x.second;
        BalanceImportedFunction *ibfunction = new BalanceImportedFunction(bmodule, bfunction);
        ibclass->methods[bfunction->name] = ibfunction;
    }

    // And for constructor
    ibclass->constructor = new BalanceImportedConstructor(bmodule, bclass);
}

void importFunctionToModule(BalanceImportedFunction *ibfunction, BalanceModule *bmodule) {
    vector<Type *> functionParameterTypes;
    vector<std::string> functionParameterNames;
    for (BalanceParameter *bparameter : ibfunction->bfunction->parameters) {
        functionParameterTypes.push_back(bparameter->type);
        functionParameterNames.push_back(bparameter->name);
    }
    ArrayRef<Type *> parametersReference(functionParameterTypes);
    FunctionType *functionType = FunctionType::get(ibfunction->bfunction->returnType, parametersReference, false);

    ibfunction->function = Function::Create(functionType, Function::ExternalLinkage, ibfunction->bfunction->name, bmodule->module);
}

void importClassToModule(BalanceImportedClass *ibclass, BalanceModule *bmodule) {
    for (auto const &x : ibclass->methods) {
        BalanceImportedFunction *ibfunction = x.second;
        vector<Type *> functionParameterTypes;
        vector<std::string> functionParameterNames;

        if (ibclass->bclass->structType != nullptr) {
            PointerType *thisPointer = ibclass->bclass->structType->getPointerTo();
            functionParameterTypes.insert(functionParameterTypes.begin(), thisPointer);
            functionParameterNames.insert(functionParameterNames.begin(), "this");
        }

        for (BalanceParameter *bparameter : ibfunction->bfunction->parameters) {
            functionParameterTypes.push_back(bparameter->type);
            functionParameterNames.push_back(bparameter->name);
        }
        ArrayRef<Type *> parametersReference(functionParameterTypes);
        FunctionType *functionType = FunctionType::get(ibfunction->bfunction->returnType, parametersReference, false);

        std::string functionNameWithClass = ibclass->bclass->name + "_" + ibfunction->bfunction->name;
        ibfunction->function = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, bmodule->module);
    }

    // Import constructor
    if (ibclass->bclass->structType != nullptr) {
        std::string constructorName = ibclass->bclass->structType->getName().str() + "_constructor";
        FunctionType *constructorType = ibclass->bclass->constructor->getFunctionType();
        ibclass->constructor->constructor = Function::Create(constructorType, Function::ExternalLinkage, constructorName, bmodule->module);
    }

    // TODO: Handle class properties as well - maybe they dont need forward declaration?
}

void createDefaultConstructor(BalanceModule *bmodule, BalanceClass *bclass) {
    std::string constructorName = bclass->structType->getName().str() + "_constructor";
    vector<Type *> functionParameterTypes;

    // TODO: Constructor should return Type of class?
    Type *returnType = getBuiltinType(new BalanceTypeString("None"));

    ArrayRef<Type *> parametersReference{bclass->structType->getPointerTo()};
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

    for (auto const &x : currentPackage->currentModule->currentClass->properties) {
        BalanceProperty *property = x.second;
        Type *propertyType = property->type;

        Value *initialValue;
        if (propertyType->isIntegerTy(1)) {
            initialValue = ConstantInt::get(getBuiltinType(new BalanceTypeString("Bool")), 0, true);
        } else if (propertyType->isIntegerTy(32)) {
            initialValue = ConstantInt::get(getBuiltinType(new BalanceTypeString("Int")), 0, true);
        } else if (propertyType->isFloatingPointTy()) {
            initialValue = ConstantFP::get(getBuiltinType(new BalanceTypeString("Double")), 0.0);
        } else if (propertyType->isPointerTy()) {
            if (propertyType->getPointerElementType()->isIntegerTy(32)) {
                initialValue = ConstantPointerNull::get(Type::getInt32PtrTy(*currentPackage->context));
            } else if (propertyType->getPointerElementType()->isIntegerTy(8)) {
                // Only used for builtins (e.g. File::filePointer) // TODO: Maybe removed and instead use i32*
                initialValue = ConstantPointerNull::get(Type::getInt8PtrTy(*currentPackage->context));
            }
        }
        // // TODO: Handle String and nullable types

        int intIndex = property->index;
        auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue, {zero, index});
        currentPackage->currentModule->builder->CreateStore(initialValue, ptr);
    }

    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*function, &llvm::errs());
    if (hasError) {
        // TODO: Throw error
        std::cout << "Error verifying default constructor for class: " << bclass->structType->getName().str() << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        exit(1);
    }
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}