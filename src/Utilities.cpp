#include "Utilities.h"
#include "BalancePackage.h"
#include "visitors/Visitor.h"
#include "models/BalanceType.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include "assert.h"

extern BalancePackage *currentPackage;

bool fileExist(std::string fileName) {
    std::ifstream infile(fileName);
    return infile.good();
}

void createImportedFunction(BalanceModule *bmodule, BalanceFunction *bfunction) {
    if (bmodule->isFunctionImported(bfunction)) {
        return;
    }

    bfunction->imports[bmodule] = new BalanceImportedFunction(bmodule, bfunction);
    bmodule->importedFunctions.push_back(bfunction);
}

void createImportedClass(BalanceModule *bmodule, BalanceType * btype) {
    if (bmodule->isTypeImported(btype)) {
        return;
    }

    bmodule->importedTypes.push_back(btype);
}

void finalizeInitializer(BalanceType * btype) {
    if (btype->isSimpleType || btype->name == "Lambda") {
        return;
    }

    vector<Type *> functionParameterTypes;
    Function * function = Function::Create(btype->getInitializer()->getLlvmFunctionType(), Function::ExternalLinkage, btype->initializer->getFullyQualifiedFunctionName(), currentPackage->currentModule->module);
    btype->getInitializer()->setLlvmFunction(function);

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    llvm::Value *thisValue = args++;
    thisValue->setName("self");

    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, btype->getInitializer()->getFunctionName() + "_body", function);
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

void registerInitializer(BalanceType * btype) {
    if (btype->isSimpleType) {
        return;
    }

    assert(btype->initializer == nullptr);

    std::string constructorName = "initializer";
    BalanceType * returnType = currentPackage->currentModule->getType("None");
    assert(returnType != nullptr);
    BalanceParameter * thisParameter = new BalanceParameter(btype, "self");
    btype->initializer = new BalanceFunction(currentPackage->currentModule, btype, constructorName, {thisParameter}, returnType);
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

    // Create BalanceFunction
    BalanceParameter * valueParameter = new BalanceParameter(btype, "value");
    std::vector<BalanceParameter *> parameters = {
        valueParameter
    };
    BalanceFunction * bfunction = new BalanceFunction(currentPackage->currentModule, btype, functionName, parameters, stringType);
    btype->addMethod(bfunction);

    // Create llvm function
    ArrayRef<Type *> parametersReference({
        btype->getReferencableType()
    });

    llvm::Function * btypeDefaultToStringFunc = Function::Create(bfunction->getLlvmFunctionType(), Function::ExternalLinkage, bfunction->getFullyQualifiedFunctionName(), currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", btypeDefaultToStringFunc);
    bfunction->setLlvmFunction(btypeDefaultToStringFunc);

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
    currentPackage->currentModule->builder->CreateCall(stringType->getInitializer()->getLlvmFunction(currentPackage->currentModule), argumentsReference);
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

// Returns whether a can be assigned to b
bool canAssignTo(ParserRuleContext * ctx, BalanceType * aType, BalanceType * bType) {
    // If a is interface, it can never be assigned to something not interface
    if (!bType->isInterface && aType->isInterface) {
        if (ctx != nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Can't assign interface to non-interface");
        }
        return false;
    }

    // If b is interface and a is not, check that a implements b
    if (bType->isInterface && !aType->isInterface) {
        if (aType->interfaces[bType->name] == nullptr) {
            if (ctx != nullptr) {
                currentPackage->currentModule->addTypeError(ctx, "Type " + aType->toString() + " does not implement interface " + bType->toString());
            }
            return false;
        }
        return true;
    }

    // Do similar checks for inheritance, once implemented
    // a can be assigned to b, if b is ancestor of a
    for (BalanceType * member : aType->getHierarchy()) {
        if (member == aType) {
            continue;
        }

        // TODO: We may need to consider generics here as well
        if (member == bType) {
            return true;
        }
    }

    if (!aType->equalTo(bType)) {
        // TODO: Should only be possible with strict null typing
        if (!bType->isSimpleType && aType->name == "None") {
            return true;
        }
        if (ctx != nullptr) {
            currentPackage->currentModule->addTypeError(ctx, aType->toString() + " cannot be assigned to " + bType->toString());
        }
        return false;
    }

    return true;
}

bool functionEqualTo(BalanceFunction * bfunction, std::string functionName, std::vector<BalanceType *> parameters) {
    if (bfunction->name != functionName) {
        return false;
    }

    if (bfunction->parameters.size() != parameters.size()) {
        return false;
    }

    int parameterStartIndex = 0;

    // If class-method, don't consider 'self' parameter.
    if (bfunction->balanceType != nullptr) {
        parameterStartIndex = 1;
    }

    for (int i = parameterStartIndex; i < bfunction->parameters.size(); i++) {
        if (!canAssignTo(nullptr, parameters[i], bfunction->parameters[i]->balanceType)) {
            return false;
        }
    }

    return true;
}

std::vector<BalanceType *> parametersToTypes(std::vector<BalanceParameter *> parameters) {
    std::vector<BalanceType *> types;
    for (BalanceParameter * bparameter : parameters) {
        types.push_back(bparameter->balanceType);
    }
    return types;
}

std::vector<BalanceType *> valuesToTypes(std::vector<BalanceValue *> values) {
    std::vector<BalanceType *> types;
    for (BalanceValue * bvalue : values) {
        types.push_back(bvalue->type);
    }
    return types;
}
