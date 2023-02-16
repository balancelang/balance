#include "Visitor.h"
#include "../BalancePackage.h"
#include "../models/BalanceType.h"
#include "../models/BalanceValue.h"
#include "../builtins/Array.h"
#include "../Utilities.h"

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

void debug_print_value(std::string message, llvm::Value * value) {
    llvm::FunctionType * printfFunctionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), true);
    FunctionCallee printfFunction = currentPackage->currentModule->module->getOrInsertFunction("printf", printfFunctionType);

    auto printArgs = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "DEBUG: %s %d\n", "debug", true), geti8StrVal(*currentPackage->currentModule->module, message.c_str(), "", false), value};
    Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateCall(printfFunction, printArgs);
}

BalanceValue * BalanceVisitor::visitAndLoad(ParserRuleContext * ctx) {
    BalanceValue * bvalue = any_cast<BalanceValue *>(visit(ctx));

    if (bvalue->isVariablePointer) {
        return new BalanceValue(bvalue->type, currentPackage->currentModule->builder->CreateLoad(bvalue->value));
    } else {
        return bvalue;
    }
}

std::any BalanceVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    std::string text = ctx->getText();
    std::string className = ctx->className->getText();

    // For each known generic use of class, visit the class
    std::vector<BalanceType *> btypes = {};

    if (ctx->classGenerics()) {
        btypes = currentPackage->currentModule->getGenericVariants(className);
    } else {
        BalanceType * btype = currentPackage->currentModule->getType(className);
        btypes.push_back(btype);
    }

    for (BalanceType * btype : btypes) {
        currentPackage->currentModule->currentType = btype;

        // Visit all class functions
        for (auto const &x : ctx->classElement()) {
            if (x->functionDefinition()) {
                visit(x);
            }
        }

        currentPackage->currentModule->currentType = nullptr;
    }

    return nullptr;
}

std::any BalanceVisitor::visitClassInitializerExpression(BalanceParser::ClassInitializerExpressionContext *ctx) {
    std::string text = ctx->getText();

    BalanceType * btype = any_cast<BalanceType *>(visit(ctx->classInitializer()->newableType()));
    if (btype == nullptr) {
        throw std::runtime_error("Failed to find type: " + btype->toString());
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
    currentPackage->currentModule->builder->CreateCall(btype->getInitializer(), argumentsReference);

    // Get constructor, if not default
    vector<BalanceType *> functionArguments = {btype};
    vector<Value *> functionValues = {structMemoryPointer};
    for (BalanceParser::ArgumentContext *argument : ctx->classInitializer()->argumentList()->argument()) {
        BalanceValue * bvalue = any_cast<BalanceValue *>(visit(argument));
        functionArguments.push_back(bvalue->type);
        functionValues.push_back(bvalue->value);
    }

    BalanceFunction * constructor = btype->getConstructor(functionArguments);
    if (constructor != nullptr) {
        ArrayRef<Value *> argumentsReference{functionValues};
        currentPackage->currentModule->builder->CreateCall(constructor->function, argumentsReference);
    }

    return new BalanceValue(btype, structMemoryPointer);
}

std::any BalanceVisitor::visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) {
    std::string text = ctx->getText();

    BalanceValue * valueMember = any_cast<BalanceValue *>(visit(ctx->member));

    // Store this value, so that visiting the access will use it as context
    currentPackage->currentModule->accessedValue = valueMember;
    BalanceValue * bvalue = any_cast<BalanceValue *>(visit(ctx->access));
    currentPackage->currentModule->accessedValue = nullptr;
    return bvalue;
}

std::any BalanceVisitor::visitWhileStatement(BalanceParser::WhileStatementContext *ctx) {
    std::any text = ctx->getText();

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
    visit(ctx->statementBlock());

    currentPackage->currentModule->currentScope = new BalanceScopeBlock(condBlock, currentPackage->currentModule->currentScope->parent);
    // At the end of while-block, jump back to the condition which may jump to mergeBlock or reiterate
    currentPackage->currentModule->builder->CreateBr(condBlock);

    // Make sure new code is added to "block" after while statement
    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(mergeBlock, currentPackage->currentModule->currentScope->parent); // TODO: Store scope in beginning of this function, and restore here?
    return nullptr;
}

std::any BalanceVisitor::visitForStatement(BalanceParser::ForStatementContext *ctx) {
    std::any text = ctx->getText();

    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();

    // Set up the 3 blocks we need: condition, loop-block and merge
    BasicBlock *condBlock = BasicBlock::Create(*currentPackage->context, "loopcond", function);
    BasicBlock *loopBlock = BasicBlock::Create(*currentPackage->context, "loop", function);
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->context, "afterloop", function);

    // for-loop setup which sets up an iterable with hasNext/getNext
    BalanceValue * iterable = any_cast<BalanceValue *>(visit(ctx->expression()));

    // Jump to condition
    currentPackage->currentModule->builder->CreateBr(condBlock);
    currentPackage->currentModule->builder->SetInsertPoint(condBlock);

    // Get the hasNext function from the type
    BalanceFunction * hasNextFunction = iterable->type->getMethod("hasNext");
    Value * hasNextValue = currentPackage->currentModule->builder->CreateCall(hasNextFunction->function, { iterable->value });

    // Create the condition - if expression is true, jump to loop block, else jump to after loop block
    currentPackage->currentModule->builder->CreateCondBr(hasNextValue, loopBlock, mergeBlock);

    // Set insert point to the loop-block so we can populate it
    currentPackage->currentModule->builder->SetInsertPoint(loopBlock);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(loopBlock, currentPackage->currentModule->currentScope);

    // Assign to loop variable and add to scope
    BalanceFunction * getNextFunction = iterable->type->getMethod("getNext");
    Value * getNextValue = currentPackage->currentModule->builder->CreateCall(getNextFunction->function, { iterable->value });

    std::string variableName = ctx->variableTypeTuple()->name->getText();
    currentPackage->currentModule->setValue(variableName, new BalanceValue(getNextFunction->returnType, getNextValue));

    // Visit for-loop body
    visit(ctx->statementBlock());

    // currentPackage->currentModule->currentScope = new BalanceScopeBlock(condBlock, currentPackage->currentModule->currentScope->parent);
    // At the end of for-body, call getNext and jump back to the condition which may jump to mergeBlock or reiterate
    currentPackage->currentModule->builder->CreateBr(condBlock);

    // Make sure new code is added to "block" after the for statement
    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(mergeBlock, currentPackage->currentModule->currentScope->parent); // TODO: Store scope in beginning of this function, and restore here?
    return nullptr;
}

std::any BalanceVisitor::visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) {
    std::string text = ctx->getText();

    BalanceValue * valueMember = any_cast<BalanceValue *>(visit(ctx->member));
    currentPackage->currentModule->currentLhsType = valueMember->type;
    BalanceValue * value = any_cast<BalanceValue *>(visit(ctx->value));
    currentPackage->currentModule->currentLhsType = nullptr;

    if (ctx->index) {
        // member=expression '[' index=expression ']' '=' value=expression

        // Index returns i32 directly, e.g. a[3] = ... returns 3 as a value
        BalanceValue * pointerIndex = any_cast<BalanceValue *>(visit(ctx->index));
        auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember->value, {zero, pointerIndex->value});
        currentPackage->currentModule->builder->CreateStore(value->value, ptr);
    } else if (ctx->access) {
        // member=expression '.' access=variable '=' value=expression
        std::string accessName = ctx->access->getText();

        // Check if property is an interface, then convert to fat pointer
        BalanceProperty * bprop = valueMember->type->getProperty(accessName);
        if (bprop->balanceType->isInterface) {
            BalanceType * fatPointerType = currentPackage->currentModule->getType("FatPointer");
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

            if (value->type->isInterface) {
                // TODO: If property is interface and value is same interface?
            } else {
                Value * vtable = value->type->interfaceVTables[bprop->balanceType->name];
                BitCastInst *bitcastVTableInstr = new BitCastInst(vtable, llvm::Type::getInt64PtrTy(*currentPackage->context));
                Value * bitcastVtableValue = currentPackage->currentModule->builder->Insert(bitcastVTableInstr);
                currentPackage->currentModule->builder->CreateStore(bitcastVtableValue, fatPointerVtablePointer);

                auto zeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
                auto indexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, valueMember->type->properties[accessName]->index, true));
                auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember->value, {zeroValue, indexValue});
                currentPackage->currentModule->builder->CreateStore(fatPointer, ptr);
            }
        } else {
            auto zeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto indexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, valueMember->type->getProperty(accessName)->index, true));
            auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember->value, {zeroValue, indexValue});
            currentPackage->currentModule->builder->CreateStore(value->value, ptr);
        }
    }

    return nullptr;
}

std::any BalanceVisitor::visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) {
    std::string text = ctx->getText();

    BalanceValue * valueMember = any_cast<BalanceValue *>(visit(ctx->member));
    BalanceValue * valueIndex = any_cast<BalanceValue *>(visit(ctx->index)); // Should return i32 for now

    auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember->value, {zero, valueIndex->value});
    Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateLoad(ptr);
    return new BalanceValue(valueMember->type->generics[0], llvmValue);
}

std::any BalanceVisitor::visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) {
    std::string text = ctx->getText();
    vector<BalanceValue *> values;
    for (BalanceParser::ExpressionContext *expression : ctx->listElements()->expression()) {
        BalanceValue * value = any_cast<BalanceValue *>(visit(expression));
        values.push_back(value);
    }

    BalanceValue * firstElement = values[0];
    auto elementSize = ConstantExpr::getSizeOf(firstElement->value->getType());

    BalanceType *arrayType = currentPackage->currentModule->getType("Array", { firstElement->type });
    if (arrayType == nullptr) {
        arrayType = currentPackage->currentModule->createGenericType("Array", { firstElement->type });
    }
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
    currentPackage->currentModule->builder->CreateCall(arrayType->getInitializer(), constructorArguments);

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

    return new BalanceValue(arrayType, arrayMemoryPointer);
}

std::any BalanceVisitor::visitIfStatement(BalanceParser::IfStatementContext *ctx) {
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

std::any BalanceVisitor::visitVariableExpression(BalanceParser::VariableExpressionContext *ctx) {
    if (ctx->variable()->SELF()) {
        BalanceValue * bvalue = currentPackage->currentModule->getValue("this");
        return bvalue;
    } else {
         std::string variableName = ctx->variable()->IDENTIFIER()->getText();

        // Check if we're accessing a property on a type
        if (currentPackage->currentModule->accessedValue != nullptr) {
            BalanceType * btype = currentPackage->currentModule->accessedValue->type;
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
            return new BalanceValue(bproperty->balanceType, currentPackage->currentModule->builder->CreateLoad(propertyPointerValue));
        }

        BalanceValue * bvalue = currentPackage->currentModule->getValue(variableName);

        // Check if it is a class property
        if (bvalue == nullptr && currentPackage->currentModule->currentType != nullptr) {
            BalanceProperty * bproperty = currentPackage->currentModule->currentType->getProperty(variableName);
            auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, bproperty->index, true));
            BalanceValue *thisValue = currentPackage->currentModule->getValue("this");
            thisValue = new BalanceValue(thisValue->type, currentPackage->currentModule->builder->CreateLoad(thisValue->value));
            Type *structType = thisValue->value->getType()->getPointerElementType();

            auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue->value, {zero, index});
            return new BalanceValue(bproperty->balanceType, currentPackage->currentModule->builder->CreateLoad(ptr));
        }
        return bvalue;
    }
}

std::any BalanceVisitor::visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) {
    std::string text = ctx->getText();
    std::string variableName = ctx->variableTypeTuple()->name->getText();

    // Set LHS-type if typed, so RHS can use it
    if (ctx->variableTypeTuple()->type) {
        BalanceType * lhsType = any_cast<BalanceType *>(visit(ctx->variableTypeTuple()->type));
        currentPackage->currentModule->currentLhsType = lhsType;
    }

    BalanceValue * bvalue = any_cast<BalanceValue *>(visit(ctx->expression()));

    if (bvalue->isVariablePointer) {
        Value * alloca = currentPackage->currentModule->builder->CreateAlloca(bvalue->value->getType()->getPointerElementType());
        currentPackage->currentModule->builder->CreateStore(bvalue->value, alloca);
    } else {
        Value * alloca = currentPackage->currentModule->builder->CreateAlloca(bvalue->value->getType());
        currentPackage->currentModule->builder->CreateStore(bvalue->value, alloca);
        bvalue = new BalanceValue(bvalue->type, alloca);
    }

    bvalue->isVariablePointer = true;

    currentPackage->currentModule->setValue(variableName, bvalue);
    currentPackage->currentModule->currentLhsType = nullptr;
    return nullptr;
}

std::any BalanceVisitor::visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) {
    std::string text = ctx->getText();
    std::string variableName = ctx->IDENTIFIER()->getText();
    BalanceValue *bvalue = any_cast<BalanceValue *>(visit(ctx->expression()));

    BalanceValue *variable = currentPackage->currentModule->getValue(variableName);
    if (variable == nullptr && currentPackage->currentModule->currentType != nullptr) {
        int intIndex = currentPackage->currentModule->currentType->getProperty(variableName)->index;
        auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));
        BalanceValue *thisValue = currentPackage->currentModule->getValue("this");
        thisValue = new BalanceValue(thisValue->type, currentPackage->currentModule->builder->CreateLoad(thisValue->value));
        Type *structType = thisValue->value->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue->value, {zero, index});
        return new BalanceValue(bvalue->type, currentPackage->currentModule->builder->CreateStore(bvalue->value, ptr));
    }

    if (bvalue->isVariablePointer) {
        llvm::Value * value = currentPackage->currentModule->builder->CreateLoad(bvalue->value);
        currentPackage->currentModule->builder->CreateStore(value, variable->value);
    } else {
        currentPackage->currentModule->builder->CreateStore(bvalue->value, variable->value);
    }

    return nullptr;
}

std::any BalanceVisitor::visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx) {
    BalanceType * boolType = currentPackage->currentModule->getType("Bool");
    BalanceValue *lhsVal = visitAndLoad(ctx->lhs);
    BalanceValue *rhsVal = visitAndLoad(ctx->rhs);

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

    return new BalanceValue(boolType, result);
}

std::any BalanceVisitor::visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) {
    BalanceValue *lhsVal = any_cast<BalanceValue *>(visit(ctx->lhs));
    BalanceValue *rhsVal = any_cast<BalanceValue *>(visit(ctx->rhs));
    BalanceType * doubleType = currentPackage->currentModule->getType("Double");

    // If either operand is float, cast other to float
    if (lhsVal->type->isFloatingPointType() || rhsVal->type->isFloatingPointType()) {
        llvm::Value * newLhsValue = nullptr;
        llvm::Value * newRhsValue = nullptr;

        if (lhsVal->type->name == "Int") {
            newLhsValue = currentPackage->currentModule->builder->CreateSIToFP(lhsVal->value, doubleType->getReferencableType());
        } else if (lhsVal->type->name == "Bool") {
            newLhsValue = currentPackage->currentModule->builder->CreateUIToFP(lhsVal->value, doubleType->getReferencableType());
        }

        if (rhsVal->type->name == "Int") {
            newRhsValue = currentPackage->currentModule->builder->CreateSIToFP(rhsVal->value, doubleType->getReferencableType());
        } else if (rhsVal->type->name == "Bool") {
            newRhsValue = currentPackage->currentModule->builder->CreateUIToFP(rhsVal->value, doubleType->getReferencableType());
        }

        lhsVal = new BalanceValue(doubleType, newLhsValue);
        rhsVal = new BalanceValue(doubleType, newRhsValue);
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

std::any BalanceVisitor::visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx) {
    BalanceValue *lhsVal = visitAndLoad(ctx->lhs);
    BalanceValue *rhsVal = visitAndLoad(ctx->rhs);
    BalanceType * doubleType = currentPackage->currentModule->getType("Double");

    // If either operand is float, cast other to float
    if (lhsVal->type->isFloatingPointType() || rhsVal->type->isFloatingPointType()) {
        llvm::Value * newLhsValue = nullptr;
        llvm::Value * newRhsValue = nullptr;

        if (lhsVal->type->name == "Int") {
            newLhsValue = currentPackage->currentModule->builder->CreateSIToFP(lhsVal->value, doubleType->getReferencableType());
        } else if (lhsVal->type->name == "Bool") {
            newLhsValue = currentPackage->currentModule->builder->CreateUIToFP(lhsVal->value, doubleType->getReferencableType());
        }

        if (rhsVal->type->name == "Int") {
            newRhsValue = currentPackage->currentModule->builder->CreateSIToFP(rhsVal->value, doubleType->getReferencableType());
        } else if (rhsVal->type->name == "Bool") {
            newRhsValue = currentPackage->currentModule->builder->CreateUIToFP(rhsVal->value, doubleType->getReferencableType());
        }

        lhsVal = new BalanceValue(doubleType, newLhsValue);
        rhsVal = new BalanceValue(doubleType, newRhsValue);
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

std::any BalanceVisitor::visitNoneLiteral(BalanceParser::NoneLiteralContext *ctx) {
    if (currentPackage->currentModule->currentLhsType == nullptr) {
        throw std::runtime_error("Failed to set LHS type for None value");
    }
    return new BalanceValue(currentPackage->currentModule->getType("None"), ConstantPointerNull::get((PointerType *) currentPackage->currentModule->currentLhsType->getReferencableType()));
}

std::any BalanceVisitor::visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) {
    std::string value = ctx->DECIMAL_INTEGER()->getText();
    Type *i32_type = IntegerType::getInt32Ty(*currentPackage->context);
    int intValue = stoi(value);
    return new BalanceValue(currentPackage->currentModule->getType("Int"), ConstantInt::get(i32_type, intValue, true));
}

std::any BalanceVisitor::visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) {
    BalanceType * boolType = currentPackage->currentModule->getType("Bool");
    if (ctx->TRUE()) {
        return new BalanceValue(boolType, ConstantInt::get(boolType->getInternalType(), 1, true));
    }
    return new BalanceValue(boolType, ConstantInt::get(boolType->getInternalType(), 0, true));
}

std::any BalanceVisitor::visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) {
    std::string value = ctx->DOUBLE()->getText();
    BalanceType * doubleType = currentPackage->currentModule->getType("Double");
    double doubleValue = stod(value);
    return new BalanceValue(doubleType, ConstantFP::get(doubleType->getInternalType(), doubleValue));
}

std::any BalanceVisitor::visitStringLiteral(BalanceParser::StringLiteralContext *ctx) {
    std::string text = ctx->STRING()->getText();
    BalanceType * stringType = currentPackage->currentModule->getType("String");
    int stringLength = text.size();

    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        stringType->getInternalType(),                          // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringType->getInternalType()), // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(stringMemoryPointer);

    ArrayRef<Value *> argumentsReference{stringMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(stringType->getInitializer(), argumentsReference);

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

    return new BalanceValue(stringType, stringMemoryPointer);
}

BalanceValue * BalanceVisitor::visitFunctionCall__print(BalanceParser::FunctionCallContext *ctx) {
    FunctionCallee printFunc = currentPackage->currentModule->getFunction("print", { currentPackage->currentModule->getType("String") })->function;
    BalanceParser::ArgumentContext *argument = ctx->argumentList()->argument().front();

    BalanceValue * bvalue = visitAndLoad(argument);
    // BalanceValue * bvalue = any_cast<BalanceValue *>(visit(argument));

    BalanceFunction * toStringFunction = bvalue->type->getMethod("toString");
    if (toStringFunction == nullptr) {
        // TODO: Can we predefine a print method? E.g. "MyClass(a=1, b=true)"
        throw std::runtime_error("Failed to find toString method for type: " + bvalue->type->toString());
    }

    auto args = ArrayRef<Value *>{bvalue->value};
    Value *stringValue = currentPackage->currentModule->builder->CreateCall(toStringFunction->function, args);

    auto printArgs = ArrayRef<Value *>{stringValue};
    Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateCall(printFunc, printArgs);
    BalanceType * noneType = currentPackage->currentModule->getType("None");
    return new BalanceValue(noneType, llvmValue);
}

std::any BalanceVisitor::visitFunctionCall(BalanceParser::FunctionCallContext *ctx) {
    std::string text = ctx->getText();
    std::string functionName = ctx->IDENTIFIER()->getText();

    if (functionName == "print") {
        // We handle print manually for now, until the "Any" type exists.
        // Then the print function can handle invoking toString on parameters
        // TODO: Or prefer overloading print with typed versions?
        return visitFunctionCall__print(ctx);
    }

    std::vector<BalanceType *> functionArgumentTypes;
    std::vector<Value *> functionArgumentValues;
    for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
        BalanceValue * bvalue = visitAndLoad(argument);
        functionArgumentTypes.push_back(bvalue->type);
        functionArgumentValues.push_back(bvalue->value);
    }

    if (currentPackage->currentModule->accessedValue == nullptr) {
        // Check if function is a variable (lambda e.g.)
        BalanceValue * bvalue = currentPackage->currentModule->getValue(functionName);
        if (bvalue != nullptr) {
            ArrayRef<Value *> argumentsReference(functionArgumentValues);
            FunctionType *FT = dyn_cast<FunctionType>(bvalue->value->getType()->getPointerElementType());
            Value * llvmValue = (Value *)currentPackage->currentModule->builder->CreateCall(FT, bvalue->value, argumentsReference);
            BalanceType * returnType = bvalue->type->generics.back();
            return new BalanceValue(returnType, llvmValue);
        } else {
            // Must be a function then
            BalanceFunction *bfunction = currentPackage->currentModule->getFunction(functionName, functionArgumentTypes);

            std::vector<Value *> functionArguments;
            int i = 0;
            for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
                BalanceValue * bvalue = any_cast<BalanceValue *>(visit(argument));
                BalanceParameter * bparameter = bfunction->parameters[i];

                if (bparameter->balanceType->isInterface) {
                    // TODO: Let BalanceValue tell us if its a FatPointer instead of pulling it out of the struct like this
                    std::string structName = bvalue->value->getType()->getPointerElementType()->getStructName().str();
                    if (structName != "FatPointer") {
                        BalanceType * fatPointerType = currentPackage->currentModule->getType("FatPointer");
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

                        Value * vtable = bvalue->type->interfaceVTables[bparameter->balanceType->name];
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
            return new BalanceValue(bfunction->returnType, llvmValue);
        }
    } else {
        // Means we're invoking function on an element

        BalanceType *btype = currentPackage->currentModule->accessedValue->type;
        if (btype == nullptr) {
            throw std::runtime_error("Failed to find type " + currentPackage->currentModule->accessedValue->type->toString());
        }

        BalanceFunction *bfunction = btype->getMethod(functionName);

        if (btype->isInterface) {
            // Get vtable function index
            int index = btype->getMethodIndex(functionName);

            // get fat pointer type
            BalanceType * fatPointerType = currentPackage->currentModule->getType("FatPointer");

            // get fat pointer 'this' argument
            auto fatPointerThisZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto fatPointerThisIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto fatPointerThisPointer = currentPackage->currentModule->builder->CreateGEP(fatPointerType->getInternalType(), currentPackage->currentModule->accessedValue->value, {fatPointerThisZeroValue, fatPointerThisIndexValue});
            Value * fatPointerThisValue = (Value *)currentPackage->currentModule->builder->CreateLoad(fatPointerThisPointer);
            BitCastInst *bitcastThisValueInstr = new BitCastInst(fatPointerThisValue, bfunction->parameters[0]->balanceType->getReferencableType());
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
            BitCastInst *bitcast = new BitCastInst(fatPointerVtableValue, PointerType::get(btype->vTableStructType, 0));
            Value * bitcastValue = currentPackage->currentModule->builder->Insert(bitcast);
            auto functionPointer = currentPackage->currentModule->builder->CreateGEP(btype->vTableStructType, bitcastValue, {functionZeroValue, functionIndexValue});
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

            return new BalanceValue(bfunction->returnType, llvmValue);
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

            return new BalanceValue(bfunction->returnType, llvmValue);
        }
    }
    // TODO: Should functions be referenced with getValue as well, so we get the closest lambda/func
    // return std::any();
}

std::any BalanceVisitor::visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) {
    std::string text = ctx->getText();
    if (ctx->expression()->getText() == "None") {
        // handled in visitFunctionDefinition, since we will not hit this method on implicit 'return None'
    } else {
        BalanceValue * bvalue = any_cast<BalanceValue *>(visit(ctx->expression()));

        BalanceFunction * bfunction = currentPackage->currentModule->currentFunction;
        BalanceLambda * blambda = currentPackage->currentModule->currentLambda;

        if ((bfunction != nullptr && bfunction->returnType->isInterface || blambda != nullptr && blambda->returnType->isInterface) && !bvalue->type->isInterface) {
            // Convert to fat pointer
            BalanceType * fatPointerType = currentPackage->currentModule->getType("FatPointer");
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

            Value * vtable = bvalue->type->interfaceVTables[currentPackage->currentModule->currentFunction->returnType->name];
            BitCastInst *bitcastVTableInstr = new BitCastInst(vtable, llvm::Type::getInt64PtrTy(*currentPackage->context));
            Value * bitcastVtableValue = currentPackage->currentModule->builder->Insert(bitcastVTableInstr);
            currentPackage->currentModule->builder->CreateStore(bitcastVtableValue, fatPointerVtablePointer);

            currentPackage->currentModule->builder->CreateRet(fatPointer);
        } else {
            currentPackage->currentModule->builder->CreateRet(bvalue->value);
        }
        currentPackage->currentModule->currentScope->isTerminated = true;
    }
    return nullptr;
}

std::any BalanceVisitor::visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx) {
    std::string text = ctx->getText();
    vector<BalanceType *> functionParameterTypeStrings;
    vector<std::string> functionParameterNames;
    vector<llvm::Type *> functionParameterTypes;

    for (BalanceParser::VariableTypeTupleContext *parameter : ctx->lambda()->parameterList()->variableTypeTuple()) {
        std::string parameterName = parameter->name->getText();
        BalanceType * btype = nullptr;
        if (parameter->type) {
            std::string typeString = parameter->type->getText();
            btype = currentPackage->currentModule->getType(typeString);
        } else {
            btype = currentPackage->currentModule->getType("Any");
        }

        functionParameterNames.push_back(parameterName);
        functionParameterTypes.push_back(btype->getReferencableType());
        functionParameterTypeStrings.push_back(btype);
    }

    // If we don't have a return type, assume none
    BalanceType * returnType;
    if (ctx->lambda()->returnType()) {
        std::string functionReturnTypeString = ctx->lambda()->returnType()->balanceType()->getText();
        returnType = currentPackage->currentModule->getType(functionReturnTypeString);
    } else {
        returnType = currentPackage->currentModule->getType("None");
    }

    ArrayRef<Type *> parametersReference(functionParameterTypes);
    FunctionType *functionType = FunctionType::get(returnType->getReferencableType(), parametersReference, false);
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

    if (returnType->name == "None") {
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

    vector<BalanceType *> lambdaGenerics;
    for (BalanceType * btype : functionParameterTypeStrings) {
        lambdaGenerics.push_back(btype);
    }
    lambdaGenerics.push_back(returnType);

    BalanceType * lambdaType = currentPackage->currentModule->getType("Lambda", lambdaGenerics);
    return new BalanceValue(lambdaType, (Value *)currentPackage->currentModule->builder->CreateLoad(p));
}

std::any BalanceVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) {
    std::string functionName = ctx->functionSignature()->IDENTIFIER()->getText();
    BalanceFunction *bfunction;

    vector<BalanceType *> functionArgumentTypes;
    for (BalanceParser::VariableTypeTupleContext *parameter : ctx->functionSignature()->parameterList()->variableTypeTuple()) {
        BalanceType * btype = any_cast<BalanceType *>(visit(parameter));
        functionArgumentTypes.push_back(btype);
    }

    if (currentPackage->currentModule->currentType != nullptr) {
        if (functionName == currentPackage->currentModule->currentType->name) {
            functionArgumentTypes.insert(functionArgumentTypes.begin(), currentPackage->currentModule->currentType); // implicit 'this')
            bfunction = currentPackage->currentModule->currentType->getConstructor(functionArgumentTypes);
        } else {
            bfunction = currentPackage->currentModule->currentType->getMethod(functionName);
        }
    } else {
        bfunction = currentPackage->currentModule->getFunction(functionName, functionArgumentTypes);
    }

    currentPackage->currentModule->currentFunction = bfunction;

    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", bfunction->function);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(functionBody, scope);

    // Add function parameter names and insert in function scope
    Function::arg_iterator args = bfunction->function->arg_begin();

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    for (BalanceParameter *parameter : bfunction->parameters) {
        llvm::Value *x = args++;
        x->setName(parameter->name);

        Value *alloca = currentPackage->currentModule->builder->CreateAlloca(x->getType());
        currentPackage->currentModule->builder->CreateStore(x, alloca);
        BalanceValue * bvalue = new BalanceValue(parameter->balanceType, alloca);
        currentPackage->currentModule->setValue(parameter->name, bvalue);
    }

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

    if (bfunction->returnType->name == "None") {
        currentPackage->currentModule->builder->CreateRetVoid();
    }

    bool hasError = verifyFunction(*bfunction->function, &llvm::errs());
    if (hasError) {
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        throw std::runtime_error("Error verifying function: " + bfunction->name);
    }

    currentPackage->currentModule->currentFunction = nullptr;
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
    currentPackage->currentModule->currentScope = scope;
    return nullptr;
}

std::any BalanceVisitor::visitMapInitializerExpression(BalanceParser::MapInitializerExpressionContext *ctx) {
    string text = ctx->getText();
    BalanceType * btype = currentPackage->currentModule->currentLhsType;
    if (btype != nullptr) {
        // Shorthand constructor syntax

        // Malloc and call constructor
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
        currentPackage->currentModule->builder->CreateCall(btype->getInitializer(), argumentsReference);

        std::vector<BalanceProperty *> bproperties = btype->getProperties();
        for(BalanceParser::MapItemContext * mapItem : ctx->mapInitializer()->mapItemList()->mapItem()) {
            std::string propertyName = mapItem->key->getText();
            BalanceProperty * bproperty = btype->getProperty(propertyName);
            BalanceValue * bvalue = any_cast<BalanceValue *>(visit(mapItem->value));

            auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
            auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, bproperty->index, true));
            auto ptr = currentPackage->currentModule->builder->CreateGEP(btype->getInternalType(), structMemoryPointer, {zero, index});

            if (bproperty->balanceType->isInterface && !bvalue->type->isInterface) {
                // TODO: Convert to interface
                BalanceType * fatPointerType = currentPackage->currentModule->getType("FatPointer");
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

                Value * vtable = bvalue->type->interfaceVTables[bproperty->balanceType->name];
                BitCastInst *bitcastVTableInstr = new BitCastInst(vtable, llvm::Type::getInt64PtrTy(*currentPackage->context));
                Value * bitcastVtableValue = currentPackage->currentModule->builder->Insert(bitcastVTableInstr);
                currentPackage->currentModule->builder->CreateStore(bitcastVtableValue, fatPointerVtablePointer);
                currentPackage->currentModule->builder->CreateStore(fatPointer, ptr);
            } else {
                currentPackage->currentModule->builder->CreateStore(bvalue->value, ptr);
            }
        }

        return new BalanceValue(btype, structMemoryPointer);
    } else {
        // Instantiate new map
        throw std::runtime_error("Map type not implemented yet :(");
    }
}

std::any BalanceVisitor::visitGenericType(BalanceParser::GenericTypeContext *ctx) {
    std::string base = ctx->IDENTIFIER()->getText();
    std::vector<BalanceType *> generics;

    for (BalanceParser::BalanceTypeContext *type: ctx->typeList()->balanceType()) {
        BalanceType * btype = any_cast<BalanceType *>(visit(type));
        generics.push_back(btype);
    }

    return currentPackage->currentModule->getType(base, generics);
}

std::any BalanceVisitor::visitSimpleType(BalanceParser::SimpleTypeContext *ctx) {
    std::string typeName = ctx->IDENTIFIER()->getText();

    if (currentPackage->currentModule->currentType != nullptr) {
        // Check if it is a class generic
        if (currentPackage->currentModule->currentType->genericsMapping.find(typeName) != currentPackage->currentModule->currentType->genericsMapping.end()) {
            return currentPackage->currentModule->currentType->genericsMapping[typeName];
        }
        for (BalanceType * genericType : currentPackage->currentModule->currentType->generics) {
            if (typeName == genericType->name) {
                return genericType;
            }
        }
    }
    return currentPackage->currentModule->getType(typeName);
}

std::any BalanceVisitor::visitNoneType(BalanceParser::NoneTypeContext *ctx) {
    return currentPackage->currentModule->getType(ctx->NONE()->getText());
}

std::any BalanceVisitor::visitRange(BalanceParser::RangeContext *ctx) {
    std::string text = ctx->getText();

    BalanceValue * fromValue = any_cast<BalanceValue *>(visit(ctx->from));
    BalanceValue * toValue = any_cast<BalanceValue *>(visit(ctx->to));

    // Get the range overloads which takes two integers
    BalanceFunction *bfunction = currentPackage->currentModule->getFunction("range", {
        currentPackage->currentModule->getType("Int"),
        currentPackage->currentModule->getType("Int")
    });
    Value * rangeResult = currentPackage->currentModule->builder->CreateCall(bfunction->function, {fromValue->value, toValue->value});

    return new BalanceValue(bfunction->returnType, rangeResult);
}
