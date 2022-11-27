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
    BalanceImportedFunction *ibfunction = new BalanceImportedFunction(bmodule, bfunction);
    bmodule->importedFunctions[bfunction->name] = ibfunction;
}

void createImportedClass(BalanceModule *bmodule, BalanceClass *bclass) {
    BalanceImportedClass *ibclass = new BalanceImportedClass(bmodule, bclass);
    bmodule->importedClasses[bclass->name->toString()] = ibclass;

    // Create BalanceImportedFunction for each class method
    for (auto const &x : bclass->getMethods()) {
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

        for (BalanceParameter *bparameter : ibfunction->bfunction->parameters) {
            functionParameterTypes.push_back(bparameter->type);
            functionParameterNames.push_back(bparameter->name);
        }
        ArrayRef<Type *> parametersReference(functionParameterTypes);
        FunctionType *functionType = FunctionType::get(ibfunction->bfunction->returnType, parametersReference, false);

        std::string functionNameWithClass = ibclass->bclass->name->toString() + "_" + ibfunction->bfunction->name;
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

    // Initialize vtable pointer
    // if (bclass->vTableStructType != nullptr) {
    //     auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    //     auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, bclass->vtableTypeIndex, true));
    //     Type *structType = thisValue->getType()->getPointerElementType();
    //     auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue, {zero, index});

    //     vector<Constant *> values;
    //     for (auto m : bclass->getMethods()) {
    //         values.push_back(m.second->function);
    //     }
    //     ArrayRef<Constant *> valuesRef(values);
    //     Constant * vTableData = ConstantStruct::get(bclass->vTableStructType, valuesRef);
    //     GlobalVariable * vTableDataVariable = new GlobalVariable(*currentPackage->currentModule->module, bclass->vTableStructType, true, GlobalValue::CommonLinkage, vTableData, bclass->name->toString() + "_vtable_data");
    //     currentPackage->currentModule->builder->CreateStore(vTableDataVariable, ptr);
    // }

    // Return void
    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*function, &llvm::errs());
    if (hasError) {
        throw std::runtime_error("Error verifying default constructor for class: " + bclass->structType->getName().str());
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