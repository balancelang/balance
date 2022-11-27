#include "Visitor.h"
#include "../BalancePackage.h"
#include "../models/BalanceClass.h"
#include "../models/BalanceValue.h"
#include "../builtins/Array.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include "antlr4-runtime.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

#include "config.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeindex>
#include <typeinfo>

using namespace antlrcpptest;

extern BalancePackage *currentPackage;

Type *getBuiltinType(BalanceTypeString * typeString) {
    if (typeString->base == "Int") {
        return Type::getInt32Ty(*currentPackage->context);
    } else if (typeString->base == "Bool") {
        return Type::getInt1Ty(*currentPackage->context);
    } else if (typeString->base == "String") {
        return currentPackage->builtins->getClass(typeString)->structType->getPointerTo();
    } else if (typeString->base == "Double") {
        return Type::getDoubleTy(*currentPackage->context);
    } else if (typeString->base == "None") {
        return Type::getVoidTy(*currentPackage->context);
    } else if (typeString->base == "Array") {
        BalanceClass * arrayClass = currentPackage->builtins->getClass(typeString);
        if (arrayClass == nullptr) {
            // TODO: Try to push this out of Visitor.cpp and make it a pass before this step
            BalanceTypeString * genericTypeString = typeString->generics[0];
            if (!genericTypeString->finalized()) {
                genericTypeString->populateTypes();
            }
            BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
            createType__Array(typeString);
            currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);

            // Import class into module
            arrayClass = currentPackage->builtins->getClass(typeString);
            createImportedClass(currentPackage->currentModule, arrayClass);
            BalanceImportedClass * ibclass = currentPackage->currentModule->getImportedClass(arrayClass->name);
            importClassToModule(ibclass, currentPackage->currentModule);
        }
        return arrayClass->structType->getPointerTo();
    } else if (typeString->base == "Lambda") {
        vector<Type *> functionParameterTypes;
        //                                              -1 since last parameter is return
        for (int i = 0; i < typeString->generics.size() - 1; i++) {
            functionParameterTypes.push_back(getBuiltinType(typeString->generics[i]));
        }

        ArrayRef<Type *> parametersReference(functionParameterTypes);

        Type * returnType = getBuiltinType(typeString->generics.back());
        return FunctionType::get(returnType, parametersReference, false)->getPointerTo();
    }

    return nullptr;
}

Constant *geti8StrVal(Module &M, char const *str, Twine const &name, bool addNull) {
    Constant *strConstant = ConstantDataArray::getString(M.getContext(), str, addNull);
    auto *GVStr = new GlobalVariable(M, strConstant->getType(), true, GlobalValue::InternalLinkage, strConstant, name);
    Constant *zero = Constant::getNullValue(IntegerType::getInt32Ty(M.getContext()));
    Constant *indices[] = {zero, zero};
    Constant *strVal = ConstantExpr::getGetElementPtr(strConstant->getType(), GVStr, indices, true);
    return strVal;
}

any BalanceVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    std::string text = ctx->getText();
    std::string className = ctx->className->getText();
    currentPackage->currentModule->currentClass = currentPackage->currentModule->classes[className];

    // Visit all class functions
    for (auto const &x : ctx->classElement()) {
        if (x->functionDefinition()) {
            visit(x);
        }
    }

    currentPackage->currentModule->currentClass = nullptr;
    return nullptr;
}

any BalanceVisitor::visitClassInitializerExpression(BalanceParser::ClassInitializerExpressionContext *ctx) {
    std::string text = ctx->getText();
    // TODO: include generic types, if any
    BalanceTypeString * className = new BalanceTypeString(ctx->classInitializer()->IDENTIFIER()->getText());

    BalanceClass *bclass;
    llvm::Function *constructor;
    if (currentPackage->currentModule->getClass(className) != nullptr) {
        // Class is in current module
        bclass = currentPackage->currentModule->getClass(className);
        constructor = bclass->constructor;
    } else if (currentPackage->currentModule->getImportedClass(className) != nullptr) {
        // Class is imported
        BalanceImportedClass *ibclass = currentPackage->currentModule->getImportedClass(className);
        bclass = ibclass->bclass;
        constructor = ibclass->constructor->constructor;
    } else {
        throw std::runtime_error("Failed to find type: " + className->toString());
    }

    auto structMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),           // input type?
        bclass->structType,                                         // output type, which we get pointer to?
        ConstantExpr::getSizeOf(bclass->structType),                // size, matches input type?
        nullptr,
        nullptr,
        ""
    );
    currentPackage->currentModule->builder->Insert(structMemoryPointer);

    ArrayRef<Value *> argumentsReference{structMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(constructor, argumentsReference);
    return new BalanceValue(className, structMemoryPointer);
}

any BalanceVisitor::visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) {
    std::string text = ctx->getText();

    BalanceValue * valueMember = any_cast<BalanceValue *>(visit(ctx->member));

    // Store this value, so that visiting the access will use it as context
    currentPackage->currentModule->accessedValue = valueMember;
    BalanceValue * bvalue = any_cast<BalanceValue *>(visit(ctx->access));
    currentPackage->currentModule->accessedValue = nullptr;
    return bvalue;
}

any BalanceVisitor::visitWhileStatement(BalanceParser::WhileStatementContext *ctx) {
    any text = ctx->getText();

    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();

    // Set up the 3 blocks we need: condition, loop-block and merge
    BasicBlock *condBlock = BasicBlock::Create(*currentPackage->context, "loopcond", function);
    BasicBlock *loopBlock = BasicBlock::Create(*currentPackage->context, "loop", function);
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->context, "afterloop", function);

    // Jump to condition
    currentPackage->currentModule->builder->CreateBr(condBlock);
    currentPackage->currentModule->builder->SetInsertPoint(condBlock);

    // while condition, e.g. (i < 5)
    BalanceValue * expression = any_cast<BalanceValue *>(visit(ctx->expression()));

    // Create the condition - if expression is true, jump to loop block, else jump to after loop block
    currentPackage->currentModule->builder->CreateCondBr(expression->value, loopBlock, mergeBlock);

    // Set insert point to the loop-block so we can populate it
    currentPackage->currentModule->builder->SetInsertPoint(loopBlock);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(loopBlock, currentPackage->currentModule->currentScope);

    // Visit the while-block statements
    visit(ctx->ifBlock());

    currentPackage->currentModule->currentScope = new BalanceScopeBlock(condBlock, currentPackage->currentModule->currentScope->parent);
    // At the end of while-block, jump back to the condition which may jump to mergeBlock or reiterate
    currentPackage->currentModule->builder->CreateBr(condBlock);

    // Make sure new code is added to "block" after while statement
    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(mergeBlock, currentPackage->currentModule->currentScope->parent); // TODO: Store scope in beginning of this function, and restore here?
    return nullptr;
}

any BalanceVisitor::visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) {
    std::string text = ctx->getText();

    BalanceValue * value = any_cast<BalanceValue *>(visit(ctx->value));

    BalanceValue * valueMember = any_cast<BalanceValue *>(visit(ctx->member));
    if (ctx->index) {
        // Index returns i32 directly, e.g. a[3] = ... returns 3 as a value
        BalanceValue * pointerIndex = any_cast<BalanceValue *>(visit(ctx->index));
        auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember->value, {zero, pointerIndex->value});
        currentPackage->currentModule->builder->CreateStore(value->value, ptr);
    } else if (ctx->access) {
        std::string accessName = ctx->access->getText();

        BalanceClass * bclass = currentPackage->currentModule->getClass(valueMember->type);
        if (bclass == nullptr) {
            BalanceImportedClass *ibclass = currentPackage->currentModule->getImportedClass(valueMember->type);
            if (ibclass == nullptr) {
                // TODO: Throw error
            } else {
                bclass = ibclass->bclass;
            }
        }

        auto zeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto indexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, bclass->properties[accessName]->index, true));
        auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember->value, {zeroValue, indexValue});
        currentPackage->currentModule->builder->CreateStore(value->value, ptr);
    }

    return nullptr;
}

any BalanceVisitor::visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) {
    std::string text = ctx->getText();

    BalanceValue * valueMember = any_cast<BalanceValue *>(visit(ctx->member));
    BalanceValue * valueIndex = any_cast<BalanceValue *>(visit(ctx->index)); // Should return i32 for now

    auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember->value, {zero, valueIndex->value});
    Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateLoad(ptr);
    return new BalanceValue(valueMember->type->generics[0], llvmValue);
}

any BalanceVisitor::visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) {
    std::string text = ctx->getText();
    vector<BalanceValue *> values;
    for (BalanceParser::ExpressionContext *expression : ctx->listElements()->expression()) {
        BalanceValue * value = any_cast<BalanceValue *>(visit(expression));
        values.push_back(value);
    }

    BalanceValue * firstElement = values[0];
    auto elementSize = ConstantExpr::getSizeOf(firstElement->value->getType());

    BalanceTypeString * arrayClassString = new BalanceTypeString("Array");

    arrayClassString->generics.push_back(firstElement->type);

    // This has the side-effect of constructing the array-generic type.
    getBuiltinType(arrayClassString);

    BalanceImportedClass *arrayClass = currentPackage->currentModule->getImportedClass(arrayClassString);
    auto arrayLength = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*currentPackage->context), values.size());

    auto memoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),           // input type?
        firstElement->value->getType(),                             // output type, which we get pointer to?
        elementSize,                                                // size, matches input type?
        arrayLength,
        nullptr,
        ""
    );
    currentPackage->currentModule->builder->Insert(memoryPointer);

    for (int i = 0; i < values.size(); i++) {
        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, i, true));
        auto ptr = currentPackage->currentModule->builder->CreateGEP(memoryPointer, {index});
        currentPackage->currentModule->builder->CreateStore(values[i]->value, ptr);
    }

    auto arrayMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),           // input type?
        arrayClass->bclass->structType,                             // output type, which we get pointer to?
        ConstantExpr::getSizeOf(arrayClass->bclass->structType),    // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(arrayMemoryPointer);
    ArrayRef<Value *> constructorArguments{arrayMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(arrayClass->constructor->constructor, constructorArguments);

    // length
    auto lengthZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto lengthIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayClass->bclass->properties["length"]->index, true));
    auto lengthGEP = currentPackage->currentModule->builder->CreateGEP(arrayClass->bclass->structType, arrayMemoryPointer, {lengthZeroValue, lengthIndexValue});
    currentPackage->currentModule->builder->CreateStore(arrayLength, lengthGEP);

    // memorypointer
    auto memoryPointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto memoryPointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayClass->bclass->properties["memoryPointer"]->index, true));
    auto memoryPointerGEP = currentPackage->currentModule->builder->CreateGEP(arrayClass->bclass->structType, arrayMemoryPointer, {memoryPointerZeroValue, memoryPointerIndexValue});
    currentPackage->currentModule->builder->CreateStore(memoryPointer, memoryPointerGEP);

    return new BalanceValue(arrayClassString, arrayMemoryPointer);
}

any BalanceVisitor::visitIfStatement(BalanceParser::IfStatementContext *ctx) {
    std::string text = ctx->getText();
    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();

    BalanceValue *expression = any_cast<BalanceValue *>(visit(ctx->expression()));
    BasicBlock *thenBlock = BasicBlock::Create(*currentPackage->context, "then", function);
    BasicBlock *elseBlock = BasicBlock::Create(*currentPackage->context, "else");
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->context, "ifcont");
    currentPackage->currentModule->builder->CreateCondBr(expression->value, thenBlock, elseBlock);

    currentPackage->currentModule->builder->SetInsertPoint(thenBlock);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(thenBlock, scope);
    visit(ctx->ifblock);

    // If current scope was already terminated by return, don't branch to mergeblock
    if (!currentPackage->currentModule->currentScope->isTerminated) {
        currentPackage->currentModule->builder->CreateBr(mergeBlock);
    }
    thenBlock = currentPackage->currentModule->builder->GetInsertBlock();
    function->getBasicBlockList().push_back(elseBlock);
    currentPackage->currentModule->builder->SetInsertPoint(elseBlock);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(elseBlock, scope);
    if (ctx->elseblock) {
        visit(ctx->elseblock);
    }

    // If current scope was already terminated by return, don't branch to mergeblock
    if (!currentPackage->currentModule->currentScope->isTerminated) {
        currentPackage->currentModule->builder->CreateBr(mergeBlock);
    }
    elseBlock = currentPackage->currentModule->builder->GetInsertBlock();
    function->getBasicBlockList().push_back(mergeBlock);
    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    currentPackage->currentModule->currentScope = scope;
    return nullptr;
}

any BalanceVisitor::visitVariableExpression(BalanceParser::VariableExpressionContext *ctx) {
    std::string variableName = ctx->variable()->IDENTIFIER()->getText();

    // Check if we're accessing a property on a type
    if (currentPackage->currentModule->accessedValue != nullptr) {
        BalanceClass *bclass = currentPackage->currentModule->getClass(currentPackage->currentModule->accessedValue->type);
        if (bclass == nullptr) {
            BalanceImportedClass *ibClass = currentPackage->currentModule->getImportedClass(currentPackage->currentModule->accessedValue->type);
            if (ibClass == nullptr) {
                throw std::runtime_error("Failed to find type: " + currentPackage->currentModule->accessedValue->type->toString());
            } else {
                bclass = ibClass->bclass;
            }
        }

        // check if it is a public property
        BalanceProperty *bproperty = bclass->getProperty(variableName);
        if (bproperty == nullptr) {
            // TODO: Handle property not existing
            // TODO: Move this check to type-checking so we can just assume this exists
        }

        if (!bproperty->isPublic) {
            // TODO: Handle property not being public (e.g. inaccessible from Balance)
        }

        Value *zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        Value *propertyIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, bproperty->index, true));
        Value *propertyPointerValue = currentPackage->currentModule->builder->CreateGEP(bclass->structType, currentPackage->currentModule->accessedValue->value, {zero, propertyIndex});
        return new BalanceValue(bproperty->stringType, currentPackage->currentModule->builder->CreateLoad(propertyPointerValue));
    }

    BalanceValue * value = currentPackage->currentModule->getValue(variableName);

    // Check if it is a class property
    if (value == nullptr && currentPackage->currentModule->currentClass != nullptr) {
        // TODO: Check if variableName is in currentClass->properties
        // TODO: Is this already checked in type-checking?
        BalanceProperty * bproperty = currentPackage->currentModule->currentClass->properties[variableName];
        auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, bproperty->index, true));
        BalanceValue *thisValue = currentPackage->currentModule->getValue("this");
        Type *structType = thisValue->value->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue->value, {zero, index});
        value = new BalanceValue(bproperty->stringType, currentPackage->currentModule->builder->CreateLoad(ptr));
    }

    // TODO: Should we pass lambda around as Pointer->Pointer->Function?
    if (value->type->isSimpleType() || value->type->base == "Lambda") {
        return value;
    } else {
        return new BalanceValue(value->type, currentPackage->currentModule->builder->CreateLoad(value->value));
    }
}

any BalanceVisitor::visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) {
    std::string text = ctx->getText();
    std::string variableName = ctx->IDENTIFIER()->getText();
    BalanceValue * value = any_cast<BalanceValue *>(visit(ctx->expression()));

    if (!value->type->isSimpleType() && value->type->base != "Lambda") {
        Value *alloca = currentPackage->currentModule->builder->CreateAlloca(value->value->getType());
        currentPackage->currentModule->builder->CreateStore(value->value, alloca);
        value = new BalanceValue(value->type, alloca);
    }
    currentPackage->currentModule->setValue(variableName, value);
    return nullptr;
}

any BalanceVisitor::visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) {
    // TODO: Reference counting could free up memory here

    std::string text = ctx->getText();
    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();
    std::string variableName = ctx->IDENTIFIER()->getText();
    BalanceValue *value = any_cast<BalanceValue *>(visit(ctx->expression()));

    BalanceValue *variable = currentPackage->currentModule->getValue(variableName);
    if (variable == nullptr && currentPackage->currentModule->currentClass != nullptr) {
        // TODO: Check if variableName is in currentClass->properties
        // TODO: Checked by type-checker?
        int intIndex = currentPackage->currentModule->currentClass->properties[variableName]->index;
        auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));
        BalanceValue *thisValue = currentPackage->currentModule->getValue("this");
        Type *structType = thisValue->value->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue->value, {zero, index});
        return new BalanceValue(value->type, currentPackage->currentModule->builder->CreateStore(value->value, ptr));
    }

    currentPackage->currentModule->setValue(variableName, value);
    return nullptr;
}

any BalanceVisitor::visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx) {
    BalanceValue *lhsVal = any_cast<BalanceValue *>(visit(ctx->lhs));
    BalanceValue *rhsVal = any_cast<BalanceValue *>(visit(ctx->rhs));

    llvm::Value * result = nullptr;
    if (ctx->LT()) {
        result = currentPackage->currentModule->builder->CreateICmpSLT(lhsVal->value, rhsVal->value, "lttmp");
    } else if (ctx->GT()) {
        result = currentPackage->currentModule->builder->CreateICmpSGT(lhsVal->value, rhsVal->value, "gttmp");
    } else if (ctx->OP_LE()) {
        result = currentPackage->currentModule->builder->CreateICmpSLE(lhsVal->value, rhsVal->value, "letmp");
    } else if (ctx->OP_GE()) {
        result = currentPackage->currentModule->builder->CreateICmpSGE(lhsVal->value, rhsVal->value, "getmp");
    }

    return new BalanceValue(new BalanceTypeString("Bool"), result);
}

any BalanceVisitor::visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) {
    BalanceValue *lhsVal = any_cast<BalanceValue *>(visit(ctx->lhs));
    BalanceValue *rhsVal = any_cast<BalanceValue *>(visit(ctx->rhs));

    // If either operand is float, cast other to float
    if (lhsVal->type->isFloatingPointType() || rhsVal->type->isFloatingPointType()) {
        llvm::Type * doubleType = getBuiltinType(new BalanceTypeString("Double"));
        llvm::Value * newLhsValue = nullptr;
        llvm::Value * newRhsValue = nullptr;

        if (lhsVal->type->base == "Int") {
            newLhsValue = currentPackage->currentModule->builder->CreateSIToFP(lhsVal->value, doubleType);
        } else if (lhsVal->type->base == "Bool") {
            newLhsValue = currentPackage->currentModule->builder->CreateUIToFP(lhsVal->value, doubleType);
        }

        if (rhsVal->type->base == "Int") {
            newRhsValue = currentPackage->currentModule->builder->CreateSIToFP(rhsVal->value, doubleType);
        } else if (rhsVal->type->base == "Bool") {
            newRhsValue = currentPackage->currentModule->builder->CreateUIToFP(rhsVal->value, doubleType);
        }

        lhsVal = new BalanceValue(new BalanceTypeString("Double"), newLhsValue);
        rhsVal = new BalanceValue(new BalanceTypeString("Double"), newRhsValue);
    }

    llvm::Value * result = nullptr;

    if (ctx->STAR()) {
        // Just check one of them, since both will be floats or none.
        if (lhsVal->value->getType()->isFloatingPointTy()) {
            result = currentPackage->currentModule->builder->CreateFMul(lhsVal->value, rhsVal->value);
        }
        result = currentPackage->currentModule->builder->CreateMul(lhsVal->value, rhsVal->value);
    } else {
        if (lhsVal->value->getType()->isFloatingPointTy()) {
            result = currentPackage->currentModule->builder->CreateFDiv(lhsVal->value, rhsVal->value);
        }
        result = currentPackage->currentModule->builder->CreateSDiv(lhsVal->value, rhsVal->value);
    }

    return new BalanceValue(lhsVal->type, result);
}

any BalanceVisitor::visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx) {
    BalanceValue *lhsVal = any_cast<BalanceValue *>(visit(ctx->lhs));
    BalanceValue *rhsVal = any_cast<BalanceValue *>(visit(ctx->rhs));

    // If either operand is float, cast other to float
    if (lhsVal->type->isFloatingPointType() || rhsVal->type->isFloatingPointType()) {
        llvm::Type * doubleType = getBuiltinType(new BalanceTypeString("Double"));
        llvm::Value * newLhsValue = nullptr;
        llvm::Value * newRhsValue = nullptr;

        if (lhsVal->type->base == "Int") {
            newLhsValue = currentPackage->currentModule->builder->CreateSIToFP(lhsVal->value, doubleType);
        } else if (lhsVal->type->base == "Bool") {
            newLhsValue = currentPackage->currentModule->builder->CreateUIToFP(lhsVal->value, doubleType);
        }

        if (rhsVal->type->base == "Int") {
            newRhsValue = currentPackage->currentModule->builder->CreateSIToFP(rhsVal->value, doubleType);
        } else if (rhsVal->type->base == "Bool") {
            newRhsValue = currentPackage->currentModule->builder->CreateUIToFP(rhsVal->value, doubleType);
        }

        lhsVal = new BalanceValue(new BalanceTypeString("Double"), newLhsValue);
        rhsVal = new BalanceValue(new BalanceTypeString("Double"), newRhsValue);
    }

    llvm::Value * result = nullptr;

    if (ctx->PLUS()) {
        // Just check one of them, since both will be floats or none.
        if (lhsVal->type->isFloatingPointType()) {
            result = currentPackage->currentModule->builder->CreateFAdd(lhsVal->value, rhsVal->value);
        }
        result = currentPackage->currentModule->builder->CreateAdd(lhsVal->value, rhsVal->value);
    } else {
        if (lhsVal->type->isFloatingPointType()) {
            result = currentPackage->currentModule->builder->CreateFSub(lhsVal->value, rhsVal->value);
        }
        result = currentPackage->currentModule->builder->CreateSub(lhsVal->value, rhsVal->value);
    }

    return new BalanceValue(lhsVal->type, result);
}

any BalanceVisitor::visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) {
    std::string value = ctx->DECIMAL_INTEGER()->getText();
    Type *i32_type = IntegerType::getInt32Ty(*currentPackage->context);
    int intValue = stoi(value);
    return new BalanceValue(new BalanceTypeString("Int"), ConstantInt::get(i32_type, intValue, true));
}

any BalanceVisitor::visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) {
    Type *type = getBuiltinType(new BalanceTypeString("Bool"));
    if (ctx->TRUE()) {
        return new BalanceValue(new BalanceTypeString("Bool"), ConstantInt::get(type, 1, true));
    }
    return new BalanceValue(new BalanceTypeString("Bool"), ConstantInt::get(type, 0, true));
}

any BalanceVisitor::visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) {
    std::string value = ctx->DOUBLE()->getText();
    Type *type = getBuiltinType(new BalanceTypeString("Double"));
    double doubleValue = stod(value);
    return new BalanceValue(new BalanceTypeString("Double"), ConstantFP::get(type, doubleValue));
}

any BalanceVisitor::visitStringLiteral(BalanceParser::StringLiteralContext *ctx) {
    std::string text = ctx->STRING()->getText();
    int stringLength = text.size();

    BalanceImportedClass *ibclass = currentPackage->currentModule->getImportedClassFromStructName("String");

    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),     // input type?
        ibclass->bclass->structType,                          // output type, which we get pointer to?
        ConstantExpr::getSizeOf(ibclass->bclass->structType), // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> argumentsReference{stringMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(ibclass->constructor->constructor, argumentsReference);

    int pointerIndex = ibclass->bclass->properties["stringPointer"]->index;
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, pointerIndex, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(ibclass->bclass->structType, stringMemoryPointer, {pointerZeroValue, pointerIndexValue});

    Value *arrayValue = currentPackage->currentModule->builder->CreateGlobalStringPtr(text);
    currentPackage->currentModule->builder->CreateStore(arrayValue, pointerGEP);

    int sizeIndex = ibclass->bclass->properties["length"]->index;
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, sizeIndex, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(ibclass->bclass->structType, stringMemoryPointer, {sizeZeroValue, sizeIndexValue});
    Value *sizeValue = (Value *)ConstantInt::get(IntegerType::getInt32Ty(*currentPackage->context), stringLength, true);
    currentPackage->currentModule->builder->CreateStore(sizeValue, sizeGEP);

    return new BalanceValue(new BalanceTypeString("String"), stringMemoryPointer);
}

BalanceValue * BalanceVisitor::visitFunctionCall__open(BalanceParser::FunctionCallContext *ctx) {
    Function *fopenFunc = currentPackage->builtins->module->getFunction("fopen"); // TODO: Move this to separate module
    BalanceClass *stringClass = currentPackage->builtins->getClassFromStructName("String");
    vector<Value *> functionArguments;
    for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
        BalanceValue * bvalue = any_cast<BalanceValue *>(visit(argument));
        functionArguments.push_back(bvalue->value);
    }

    Value *zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value *stringPointerIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["stringPointer"]->index, true));
    Value *pathPointerValue = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, functionArguments[0], {zero, stringPointerIndex});
    Value *pathValue = currentPackage->currentModule->builder->CreateLoad(pathPointerValue);
    Value *modePointerValue = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, functionArguments[1], {zero, stringPointerIndex});
    Value *modeValue = currentPackage->currentModule->builder->CreateLoad(modePointerValue);

    auto args = ArrayRef<Value *>({pathValue, modeValue});
    Value *filePointer = currentPackage->currentModule->builder->CreateCall(fopenFunc, args);

    // Create File struct which holds this and return pointer to the struct
    BalanceClass *fileClass = currentPackage->builtins->classes["File"];

    auto structSize = ConstantExpr::getSizeOf(fileClass->structType);
    auto pointer = llvm::CallInst::CreateMalloc(currentPackage->currentModule->builder->GetInsertBlock(), fileClass->structType->getPointerTo(), fileClass->structType, structSize, nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(pointer);

    ArrayRef<Value *> argumentsReference{pointer};
    currentPackage->currentModule->builder->CreateCall(fileClass->constructor, argumentsReference); // TODO: should it have a constructor?

    // Get reference to 0th property (filePointer) and assign
    int intIndex = fileClass->properties["filePointer"]->index;
    auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));

    auto ptr = currentPackage->currentModule->builder->CreateGEP(fileClass->structType, pointer, {zero, index});
    currentPackage->currentModule->builder->CreateStore(filePointer, ptr);

    return new BalanceValue(new BalanceTypeString("File"), pointer);
}

BalanceValue * BalanceVisitor::visitFunctionCall__print(BalanceParser::FunctionCallContext *ctx) {
    FunctionCallee printFunc = currentPackage->currentModule->getImportedFunction("print")->function;
    BalanceParser::ArgumentContext *argument = ctx->argumentList()->argument().front();
    BalanceValue * value = any_cast<BalanceValue *>(visit(argument));
    Function * toStringFunction = nullptr;
    BalanceClass * bclass = currentPackage->currentModule->getClass(value->type);
    if (bclass == nullptr) {
        BalanceImportedClass * ibclass = currentPackage->currentModule->getImportedClass(value->type);
        if (ibclass != nullptr) {
            toStringFunction = ibclass->methods["toString"]->function;
        }
    } else {
        toStringFunction = bclass->getMethod("toString")->function;
    }

    if (toStringFunction == nullptr) {
        throw std::runtime_error("Failed to find toString method for type: " + value->type->toString());
    }

    auto args = ArrayRef<Value *>{value->value};
    Value *stringValue = currentPackage->currentModule->builder->CreateCall(toStringFunction, args);

    auto printArgs = ArrayRef<Value *>{stringValue};
    Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateCall(printFunc, printArgs);
    return new BalanceValue(new BalanceTypeString("None"), llvmValue);
}

any BalanceVisitor::visitFunctionCall(BalanceParser::FunctionCallContext *ctx) {
    std::string text = ctx->getText();
    std::string functionName = ctx->IDENTIFIER()->getText();

    if (functionName == "print") {
        // We handle print manually for now, until the "Any" type exists.
        // Then the print function can handle invoking toString on parameters
        // TODO: Or prefer overloading print with typed versions?
        return visitFunctionCall__print(ctx);
    }

    if (functionName == "open") {
        // TODO: Same as above
        return visitFunctionCall__open(ctx);
    }

    if (currentPackage->currentModule->accessedValue == nullptr) {
        // Check if function is a variable (lambda e.g.)
        BalanceValue * bvalue = currentPackage->currentModule->getValue(functionName);
        if (bvalue != nullptr) {
            vector<Value *> functionArguments;
            for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
                BalanceValue * bvalue = any_cast<BalanceValue *>(visit(argument));
                functionArguments.push_back(bvalue->value);
            }

            ArrayRef<Value *> argumentsReference(functionArguments);
            FunctionType *FT = dyn_cast<FunctionType>(bvalue->value->getType()->getPointerElementType());
            Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateCall(FT, bvalue->value, argumentsReference);
            // TODO: Find returnType of lambda
            BalanceTypeString * returnType = bvalue->type->generics.back();
            return new BalanceValue(returnType, llvmValue);
        } else {
            // Must be a function then
            FunctionCallee function;

            BalanceTypeString * returnType = nullptr;
            BalanceFunction *bfunction = currentPackage->currentModule->getFunction(functionName);
            if (bfunction == nullptr) {
                BalanceImportedFunction *ibfunction = currentPackage->currentModule->getImportedFunction(functionName);
                if (ibfunction == nullptr) {
                    // TODO: Throw error
                } else {
                    function = ibfunction->function;
                    returnType = ibfunction->bfunction->returnTypeString;
                }
            } else {
                function = bfunction->function;
                returnType = bfunction->returnTypeString;
            }

            vector<Value *> functionArguments;
            for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
                BalanceValue * bvalue = any_cast<BalanceValue *>(visit(argument));
                functionArguments.push_back(bvalue->value);
            }

            ArrayRef<Value *> argumentsReference(functionArguments);
            Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateCall(function, argumentsReference);
            return new BalanceValue(returnType, llvmValue);
        }
    } else {
        // Means we're invoking function on an element

        BalanceFunction * bfunction = nullptr;
        BalanceClass *bclass = currentPackage->currentModule->getClass(currentPackage->currentModule->accessedValue->type);
        if (bclass == nullptr) {
            BalanceImportedClass *ibClass = currentPackage->currentModule->getImportedClass(currentPackage->currentModule->accessedValue->type);
            if (ibClass == nullptr) {
                BalanceInterface * binterface = currentPackage->currentModule->getInterface(currentPackage->currentModule->accessedValue->type->base);
                if (binterface == nullptr) {
                    throw std::runtime_error("Failed to find method " + functionName + " for type: " + currentPackage->currentModule->accessedValue->type->toString());
                } else {
                    // Look up function in vtable

                    // vtable is always stored as 0th element in struct
                    StructType * structType = dyn_cast<StructType>(currentPackage->currentModule->accessedValue->value->getType());

                    auto vtableZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
                    auto vtableIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
                    auto vtableGEP = currentPackage->currentModule->builder->CreateGEP(structType, currentPackage->currentModule->accessedValue->value, {vtableZeroValue, vtableIndexValue});
                    auto vtableValue = currentPackage->currentModule->builder->CreateLoad(vtableGEP);

                    int i = 123;
                }
            } else {
                bfunction = ibClass->methods[functionName]->bfunction;
            }
        } else {
            bfunction = bclass->getMethod(functionName);
        }


        vector<Value *> functionArguments;
        // Add "this" as first argument
        functionArguments.push_back(currentPackage->currentModule->accessedValue->value);

        // Don't consider accessedValue when parsing arguments
        BalanceValue * backup = currentPackage->currentModule->accessedValue;
        currentPackage->currentModule->accessedValue = nullptr;
        for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
            BalanceValue * bvalue = any_cast<BalanceValue *>(visit(argument));
            functionArguments.push_back(bvalue->value);
        }
        currentPackage->currentModule->accessedValue = backup;

        ArrayRef<Value *> argumentsReference(functionArguments);
        Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateCall(bfunction->function->getFunctionType(), (Value *)bfunction->function, argumentsReference);

        return new BalanceValue(bfunction->returnTypeString, llvmValue);
    }
    // TODO: Should functions be referenced with getValue as well, so we get the closest lambda/func
    // return any();
}

any BalanceVisitor::visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) {
    std::string text = ctx->getText();
    if (ctx->expression()->getText() == "None") {
        // handled in visitFunctionDefinition, since we will not hit this method on implicit 'return None'
    } else {
        BalanceValue * bvalue = any_cast<BalanceValue *>(visit(ctx->expression()));
        currentPackage->currentModule->builder->CreateRet(bvalue->value);
        currentPackage->currentModule->currentScope->isTerminated = true;
    }
    return nullptr;
}

any BalanceVisitor::visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx) {
    std::string text = ctx->getText();
    vector<BalanceTypeString *> functionParameterTypeStrings;
    vector<std::string> functionParameterNames;
    vector<llvm::Type *> functionParameterTypes;
    for (BalanceParser::ParameterContext *parameter : ctx->lambda()->parameterList()->parameter()) {
        std::string parameterName = parameter->identifier->getText();
        std::string typeString = parameter->type->getText();
        BalanceTypeString * bTypeString = new BalanceTypeString(typeString);
        functionParameterNames.push_back(parameterName);
        functionParameterTypes.push_back(getBuiltinType(bTypeString));
        functionParameterTypeStrings.push_back(bTypeString);
    }

    // If we don't have a return type, assume none
    BalanceTypeString * returnType;
    if (ctx->lambda()->returnType()) {
        std::string functionReturnTypeString = ctx->lambda()->returnType()->balanceType()->getText();
        returnType = new BalanceTypeString(functionReturnTypeString); // TODO: Handle unknown type
    } else {
        returnType = new BalanceTypeString("None");
    }

    ArrayRef<Type *> parametersReference(functionParameterTypes);
    FunctionType *functionType = FunctionType::get(getBuiltinType(returnType), parametersReference, false);
    Function *function = Function::Create(functionType, Function::ExternalLinkage, "", currentPackage->currentModule->module);

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    for (int i = 0; i < functionParameterNames.size(); i++) {
        llvm::Value *x = args++;
        x->setName(functionParameterNames[i]);
        BalanceValue * bvalue = new BalanceValue(functionParameterTypeStrings[i], x);
        currentPackage->currentModule->setValue(functionParameterNames[i], bvalue);
    }

    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, "", function);
    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);
    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(functionBody, scope);

    visit(ctx->lambda()->functionBlock());

    if (returnType->base == "None") {
        currentPackage->currentModule->builder->CreateRetVoid();
    }

    bool hasError = verifyFunction(*function, &llvm::errs());
    if (hasError) {
        // TODO: Throw error and give more context
        std::cout << "Error verifying lambda expression: " << text << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        // exit(1);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
    currentPackage->currentModule->currentScope = scope;

    // Create alloca so we can return it as expression
    // TODO: Create malloc?
    llvm::Value *p = currentPackage->currentModule->builder->CreateAlloca(function->getType());
    currentPackage->currentModule->builder->CreateStore(function, p, false);

    vector<BalanceTypeString *> lambdaGenerics;
    for (BalanceTypeString * btype : functionParameterTypeStrings) {
        lambdaGenerics.push_back(btype);
    }
    lambdaGenerics.push_back(returnType);

    return new BalanceValue(new BalanceTypeString("Lambda", lambdaGenerics), (Value *)currentPackage->currentModule->builder->CreateLoad(p));
}

any BalanceVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) {
    std::string functionName = ctx->functionSignature()->IDENTIFIER()->getText();
    BalanceFunction *bfunction;

    if (currentPackage->currentModule->currentClass != nullptr) {
        bfunction = currentPackage->currentModule->currentClass->getMethod(functionName);
    } else {
        bfunction = currentPackage->currentModule->getFunction(functionName);
    }

    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", bfunction->function);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(functionBody, scope);

    // Add function parameter names and insert in function scope
    Function::arg_iterator args = bfunction->function->arg_begin();

    for (BalanceParameter *parameter : bfunction->parameters) {
        llvm::Value *x = args++;
        x->setName(parameter->name);
        BalanceValue * bvalue = new BalanceValue(parameter->balanceTypeString, x);
        currentPackage->currentModule->setValue(parameter->name, bvalue);
    }

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    visit(ctx->functionBlock());

    /* BEGIN Remove empty blocks */
    vector<BasicBlock *> removes;
    for (Function::iterator b = bfunction->function->begin(), be = bfunction->function->end(); b != be; ++b) {
        BasicBlock* BB = &*b;
        if (BB->size() == 0) {
            removes.push_back(BB);
        }
    }

    for (BasicBlock* BB : removes) {
        BB->removeFromParent();
    }
    /* END Remove empty blocks */

    if (bfunction->returnType->isVoidTy()) {
        currentPackage->currentModule->builder->CreateRetVoid();
    }

    bool hasError = verifyFunction(*bfunction->function, &llvm::errs());
    if (hasError) {
        // TODO: Throw error
        std::cout << "Error verifying function: " << bfunction->name << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        // exit(1);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
    currentPackage->currentModule->currentScope = scope;
    return nullptr;
}