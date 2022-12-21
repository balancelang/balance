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
        return currentPackage->builtins->getType(typeString)->getReferencableType();
    } else if (typeString->base == "Double") {
        return Type::getDoubleTy(*currentPackage->context);
    } else if (typeString->base == "None") {
        return Type::getVoidTy(*currentPackage->context);
    } else if (typeString->base == "Array") {
        BalanceType * arrayType = currentPackage->builtins->getType(typeString);
        if (arrayType == nullptr) {
            // TODO: Try to push this out of Visitor.cpp and make it a pass before this step
            BalanceTypeString * genericTypeString = typeString->generics[0];
            if (!genericTypeString->finalized()) {
                genericTypeString->populateTypes();
            }
            BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
            createType__Array(typeString);
            currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);

            // Import class into module
            arrayType = currentPackage->builtins->getType(typeString);
            createImportedClass(currentPackage->currentModule, (BalanceClass *) arrayType);
        }
        return arrayType->getReferencableType();
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

void debug_print_value(std::string message, llvm::Value * value) {
    llvm::FunctionType * printfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), true);
    FunctionCallee printfFunction = currentPackage->currentModule->module->getOrInsertFunction("printf", printfFunctionType);

    auto printArgs = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "DEBUG: %s %d\n", "debug", true), geti8StrVal(*currentPackage->currentModule->module, message.c_str(), "", false), value};
    Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateCall(printfFunction, printArgs);
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

    BalanceType * btype = currentPackage->currentModule->getType(className);
    if (btype == nullptr) {
        throw std::runtime_error("Failed to find type: " + className->toString());
    }

    auto structMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),           // input type?
        btype->getInternalType(),                                   // output type, which we get pointer to?
        ConstantExpr::getSizeOf(btype->getInternalType()),          // size, matches input type?
        nullptr,
        nullptr,
        ""
    );
    currentPackage->currentModule->builder->Insert(structMemoryPointer);

    ArrayRef<Value *> argumentsReference{structMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(btype->getConstructor(), argumentsReference);
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

        BalanceType * valueMemberType = currentPackage->currentModule->getType(valueMember->type);
        if (valueMemberType == nullptr) {
            throw std::runtime_error("Failed to find type: " + valueMemberType->name->toString());
        }

        // Check if property is an interface, then convert to fat pointer
        BalanceProperty * bprop = valueMemberType->getProperty(accessName);
        if (bprop->stringType->isInterfaceType()) {
            BalanceType * fatPointerType = currentPackage->currentModule->getType(new BalanceTypeString("FatPointer"));
            llvm::Value * fatPointer = currentPackage->currentModule->builder->CreateAlloca(fatPointerType->getInternalType());

            // set fat pointer 'this' argument
            auto fatPointerThisZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto fatPointerThisIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto fatPointerThisPointer = currentPackage->currentModule->builder->CreateGEP(fatPointerType->getInternalType(), fatPointer, {fatPointerThisZeroValue, fatPointerThisIndexValue});

            BitCastInst *bitcastThisValueInstr = new BitCastInst(value->value, llvm::Type::getInt64PtrTy(*currentPackage->context));
            Value * bitcastThisValue = currentPackage->currentModule->builder->Insert(bitcastThisValueInstr);
            currentPackage->currentModule->builder->CreateStore(bitcastThisValue, fatPointerThisPointer);

            // set fat pointer 'vtable' argument
            auto fatPointerVtableZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto fatPointerVtableIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true));
            auto fatPointerVtablePointer = currentPackage->currentModule->builder->CreateGEP(fatPointerType->getInternalType(), fatPointer, {fatPointerVtableZeroValue, fatPointerVtableIndexValue});

            BalanceType * btype = currentPackage->currentModule->getType(value->type);
            if (btype->isInterface()) {
                // TODO: If property is interface and value is same interface?
            } else {
                BalanceClass * bclass = (BalanceClass *) btype;
                Value * vtable = bclass->interfaceVTables[bprop->stringType->base];
                BitCastInst *bitcastVTableInstr = new BitCastInst(vtable, llvm::Type::getInt64PtrTy(*currentPackage->context));
                Value * bitcastVtableValue = currentPackage->currentModule->builder->Insert(bitcastVTableInstr);
                currentPackage->currentModule->builder->CreateStore(bitcastVtableValue, fatPointerVtablePointer);

                auto zeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
                auto indexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, valueMemberType->properties[accessName]->index, true));
                auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember->value, {zeroValue, indexValue});
                currentPackage->currentModule->builder->CreateStore(fatPointer, ptr);
            }
        } else {
            auto zeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto indexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, valueMemberType->properties[accessName]->index, true));
            auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember->value, {zeroValue, indexValue});
            currentPackage->currentModule->builder->CreateStore(value->value, ptr);
        }
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

    BalanceType *arrayType = currentPackage->currentModule->getType(arrayClassString);
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
        arrayType->getInternalType(),                               // output type, which we get pointer to?
        ConstantExpr::getSizeOf(arrayType->getInternalType()),      // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(arrayMemoryPointer);
    ArrayRef<Value *> constructorArguments{arrayMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(arrayType->getConstructor(), constructorArguments);

    // length
    auto lengthZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto lengthIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayType->properties["length"]->index, true));
    auto lengthGEP = currentPackage->currentModule->builder->CreateGEP(arrayType->getInternalType(), arrayMemoryPointer, {lengthZeroValue, lengthIndexValue});
    currentPackage->currentModule->builder->CreateStore(arrayLength, lengthGEP);

    // memorypointer
    auto memoryPointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto memoryPointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayType->properties["memoryPointer"]->index, true));
    auto memoryPointerGEP = currentPackage->currentModule->builder->CreateGEP(arrayType->getInternalType(), arrayMemoryPointer, {memoryPointerZeroValue, memoryPointerIndexValue});
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
        BalanceType * btype = currentPackage->currentModule->getType(currentPackage->currentModule->accessedValue->type);
        if (btype == nullptr) {
            throw std::runtime_error("Failed to find type: " + currentPackage->currentModule->accessedValue->type->toString());
        }

        // Check if it is a public property
        BalanceProperty *bproperty = btype->getProperty(variableName);
        if (!bproperty->isPublic) {
            throw std::runtime_error("Attempted to access internal property: " + bproperty->name);
        }

        Value *zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        Value *propertyIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, bproperty->index, true));
        Value *propertyPointerValue = currentPackage->currentModule->builder->CreateGEP(btype->getInternalType(), currentPackage->currentModule->accessedValue->value, {zero, propertyIndex});
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

    // TODO: Figure out how we represent values generally - e.g. can we avoid pointer-to-pointers generally?
    if (value->value->getType()->isPointerTy() && value->value->getType()->getPointerElementType()->isPointerTy()) {
        return new BalanceValue(value->type, currentPackage->currentModule->builder->CreateLoad(value->value));
    }
    return value;
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

    BalanceType * stringType = currentPackage->currentModule->getType(new BalanceTypeString("String"));

    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringType->getInternalType(),                          // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringType->getInternalType()), // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> argumentsReference{stringMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(stringType->getConstructor(), argumentsReference);

    int pointerIndex = stringType->properties["stringPointer"]->index;
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, pointerIndex, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), stringMemoryPointer, {pointerZeroValue, pointerIndexValue});

    Value *arrayValue = currentPackage->currentModule->builder->CreateGlobalStringPtr(text);
    currentPackage->currentModule->builder->CreateStore(arrayValue, pointerGEP);

    int sizeIndex = stringType->properties["length"]->index;
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, sizeIndex, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(stringType->getInternalType(), stringMemoryPointer, {sizeZeroValue, sizeIndexValue});
    Value *sizeValue = (Value *)ConstantInt::get(IntegerType::getInt32Ty(*currentPackage->context), stringLength, true);
    currentPackage->currentModule->builder->CreateStore(sizeValue, sizeGEP);

    return new BalanceValue(new BalanceTypeString("String"), stringMemoryPointer);
}

BalanceValue * BalanceVisitor::visitFunctionCall__print(BalanceParser::FunctionCallContext *ctx) {
    FunctionCallee printFunc = currentPackage->currentModule->getFunction("print")->function;
    BalanceParser::ArgumentContext *argument = ctx->argumentList()->argument().front();
    BalanceValue * value = any_cast<BalanceValue *>(visit(argument));

    BalanceType * btype = currentPackage->currentModule->getType(value->type);
    if (btype == nullptr) {
        throw std::runtime_error("Failed to find type: " + value->type->toString());
    }

    BalanceFunction * toStringFunction = btype->getMethod("toString");
    if (toStringFunction == nullptr) {
        // TODO: Can we predefine a print method? E.g. "MyClass(a=1, b=true)"
        throw std::runtime_error("Failed to find toString method for type: " + value->type->toString());
    }

    auto args = ArrayRef<Value *>{value->value};
    Value *stringValue = currentPackage->currentModule->builder->CreateCall(toStringFunction->function, args);

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
            BalanceTypeString * returnType = bvalue->type->generics.back();
            return new BalanceValue(returnType, llvmValue);
        } else {
            // Must be a function then
            BalanceFunction *bfunction = currentPackage->currentModule->getFunction(functionName);
            if (bfunction == nullptr) {
                throw std::runtime_error("Failed to find function " + functionName);
            }

            vector<Value *> functionArguments;
            int i = 0;
            for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
                BalanceValue * bvalue = any_cast<BalanceValue *>(visit(argument));
                BalanceParameter * bparameter = bfunction->parameters[i];

                if (bparameter->balanceTypeString->isInterfaceType()) {
                    // TODO: Let BalanceValue tell us if its a FatPointer instead of pulling it out of the struct like this
                    std::string structName = bvalue->value->getType()->getPointerElementType()->getStructName().str();
                    if (structName != "FatPointer") {
                        BalanceType * fatPointerType = currentPackage->currentModule->getType(new BalanceTypeString("FatPointer"));
                        llvm::Value * fatPointer = currentPackage->currentModule->builder->CreateAlloca(fatPointerType->getInternalType());

                        // set fat pointer 'this' argument
                        auto fatPointerThisZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
                        auto fatPointerThisIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
                        auto fatPointerThisPointer = currentPackage->currentModule->builder->CreateGEP(fatPointerType->getInternalType(), fatPointer, {fatPointerThisZeroValue, fatPointerThisIndexValue});

                        BitCastInst *bitcastThisValueInstr = new BitCastInst(bvalue->value, llvm::Type::getInt64PtrTy(*currentPackage->context));
                        Value * bitcastThisValue = currentPackage->currentModule->builder->Insert(bitcastThisValueInstr);
                        currentPackage->currentModule->builder->CreateStore(bitcastThisValue, fatPointerThisPointer);

                        // set fat pointer 'vtable' argument
                        auto fatPointerVtableZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
                        auto fatPointerVtableIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true));
                        auto fatPointerVtablePointer = currentPackage->currentModule->builder->CreateGEP(fatPointerType->getInternalType(), fatPointer, {fatPointerVtableZeroValue, fatPointerVtableIndexValue});

                        BalanceClass *bclass = (BalanceClass *) currentPackage->currentModule->getType(bvalue->type);

                        Value * vtable = bclass->interfaceVTables[bparameter->balanceTypeString->base];
                        BitCastInst *bitcastVTableInstr = new BitCastInst(vtable, llvm::Type::getInt64PtrTy(*currentPackage->context));
                        Value * bitcastVtableValue = currentPackage->currentModule->builder->Insert(bitcastVTableInstr);
                        currentPackage->currentModule->builder->CreateStore(bitcastVtableValue, fatPointerVtablePointer);
                        functionArguments.push_back(fatPointer);
                    } else {
                        functionArguments.push_back(bvalue->value);
                    }
                } else {
                    functionArguments.push_back(bvalue->value);
                }
                i++;
            }

            ArrayRef<Value *> argumentsReference(functionArguments);
            Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateCall(bfunction->function, argumentsReference);
            return new BalanceValue(bfunction->returnTypeString, llvmValue);
        }
    } else {
        // Means we're invoking function on an element

        BalanceType *btype = currentPackage->currentModule->getType(currentPackage->currentModule->accessedValue->type);
        if (btype == nullptr) {
            throw std::runtime_error("Failed to find type " + currentPackage->currentModule->accessedValue->type->toString());
        }

        BalanceFunction *bfunction = btype->getMethod(functionName);

        if (btype->isInterface()) {
            // Get vtable function index
            int index = btype->getMethodIndex(functionName);

            // get fat pointer type
            BalanceType * fatPointerType = currentPackage->currentModule->getType(new BalanceTypeString("FatPointer"));

            // get fat pointer 'this' argument
            auto fatPointerThisZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto fatPointerThisIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto fatPointerThisPointer = currentPackage->currentModule->builder->CreateGEP(fatPointerType->getInternalType(), currentPackage->currentModule->accessedValue->value, {fatPointerThisZeroValue, fatPointerThisIndexValue});
            Value * fatPointerThisValue = (Value *)currentPackage->currentModule->builder->CreateLoad(fatPointerThisPointer);
            BitCastInst *bitcastThisValueInstr = new BitCastInst(fatPointerThisValue, bfunction->parameters[0]->type);
            Value * bitcastThisValue = currentPackage->currentModule->builder->Insert(bitcastThisValueInstr);

            // get fat pointer 'vtable' argument
            auto fatPointerVtableZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto fatPointerVtableIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true));
            auto fatPointerVtablePointer = currentPackage->currentModule->builder->CreateGEP(fatPointerType->getInternalType(), currentPackage->currentModule->accessedValue->value, {fatPointerVtableZeroValue, fatPointerVtableIndexValue});
            Value * fatPointerVtableValue = (Value *)currentPackage->currentModule->builder->CreateLoad(fatPointerVtablePointer);

            // Create gep for function
            auto functionZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto functionIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, index, true));

            // Bitcast the int64* to a vTableStructType*
            BalanceInterface * binterface = (BalanceInterface *) btype;
            BitCastInst *bitcast = new BitCastInst(fatPointerVtableValue, PointerType::get(binterface->vTableStructType, 0));
            Value * bitcastValue = currentPackage->currentModule->builder->Insert(bitcast);
            auto functionPointer = currentPackage->currentModule->builder->CreateGEP(binterface->vTableStructType, bitcastValue, {functionZeroValue, functionIndexValue});
            Value * functionValue = (Value *)currentPackage->currentModule->builder->CreateLoad(functionPointer);

            // create call to function
            vector<Value *> functionArguments;
            // Add "this" as first argument
            functionArguments.push_back(bitcastThisValue);

            // Don't consider accessedValue when parsing arguments
            BalanceValue * backup = currentPackage->currentModule->accessedValue;
            currentPackage->currentModule->accessedValue = nullptr;
            for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
                BalanceValue * bvalue = any_cast<BalanceValue *>(visit(argument));
                functionArguments.push_back(bvalue->value);
            }
            currentPackage->currentModule->accessedValue = backup;

            ArrayRef<Value *> argumentsReference(functionArguments);

            Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateCall(bfunction->function->getFunctionType(), functionValue, argumentsReference);

            return new BalanceValue(bfunction->returnTypeString, llvmValue);
        } else {
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