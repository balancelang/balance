#include "../headers/Visitor.h"
#include "../headers/Package.h"

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

#include "clang/Driver/Driver.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Driver/Compilation.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "antlr4-runtime.h"

#include "config.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeinfo>
#include <typeindex>

using namespace antlrcpptest;

extern BalancePackage *currentPackage;

// Used to store e.g. 'x' in 'x.toString()', so we know 'toString()' is attached to x.
llvm::Value *accessedValue;

void LogError(std::string errorMessage)
{
    fprintf(stderr, "Error: %s\n", errorMessage.c_str());
}

llvm::Value *anyToValue(any anyVal)
{
    return any_cast<llvm::Value *>(anyVal);
}

Type *
getBuiltinType(std::string typeString)
{
    if (typeString == "Int")
    {
        return Type::getInt32Ty(*currentPackage->currentModule->context);
    }
    else if (typeString == "Bool")
    {
        return Type::getInt1Ty(*currentPackage->currentModule->context);
    }
    else if (typeString == "String")
    {
        return Type::getInt8PtrTy(*currentPackage->currentModule->context);
    }
    else if (typeString == "Double")
    {
        return Type::getDoubleTy(*currentPackage->currentModule->context);
    }
    else if (typeString == "None")
    {
        return Type::getVoidTy(*currentPackage->currentModule->context);
    }

    return nullptr;
}

Constant *geti8StrVal(Module &M, char const *str, Twine const &name)
{
    Constant *strConstant = ConstantDataArray::getString(M.getContext(), str);
    auto *GVStr = new GlobalVariable(M, strConstant->getType(), true, GlobalValue::InternalLinkage, strConstant, name);
    Constant *zero = Constant::getNullValue(IntegerType::getInt32Ty(M.getContext()));
    Constant *indices[] = {zero, zero};
    Constant *strVal = ConstantExpr::getGetElementPtr(strConstant->getType(), GVStr, indices, true);
    return strVal;
}

void createDefaultConstructor(StructType *classValue)
{
    std::string constructorName = classValue->getName().str() + "_constructor";
    vector<Type *> functionParameterTypes;

    // TODO: Constructor should return Type of class?
    Type *returnType = getBuiltinType("None");

    ArrayRef<Type *> parametersReference{classValue->getPointerTo()};
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
    Function *function = Function::Create(functionType, Function::ExternalLinkage, constructorName, currentPackage->currentModule->module);
    currentPackage->currentModule->currentClass->constructor = function;

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    llvm::Value *thisValue = args++;
    thisValue->setName("this");

    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->currentModule->context, constructorName + "_body", function);
    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    for (auto const &x : currentPackage->currentModule->currentClass->properties)
    {
        BalanceProperty * property = x.second;
        Type *propertyType = property->type;

        Value *initialValue;
        if (propertyType->isIntegerTy(1))
        {
            initialValue = ConstantInt::get(getBuiltinType("Bool"), 0, true);
        }
        else if (propertyType->isIntegerTy(32))
        {
            initialValue = ConstantInt::get(getBuiltinType("Int"), 0, true);
        }
        else if (propertyType->isFloatingPointTy())
        {
            initialValue = ConstantFP::get(getBuiltinType("Double"), 0.0);
        }
        // // TODO: Handle String and nullable types

        int intIndex = property->index;
        auto zero = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, intIndex, true));
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue, {zero, index});
        currentPackage->currentModule->builder->CreateStore(initialValue, ptr);
    }

    currentPackage->currentModule->builder->CreateRetVoid();

    bool hasError = verifyFunction(*function);
    if (hasError) {
        // TODO: Throw error
        std::cout << "Error verifying default constructor for class: " << classValue->getName().str() << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        exit(1);
    }
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

any BalanceVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx)
{
    std::string text = ctx->getText();
    std::string className = ctx->className->getText();
    currentPackage->currentModule->currentClass = currentPackage->currentModule->classes[className];

    createDefaultConstructor(currentPackage->currentModule->currentClass->structType);

    // Visit all class functions
    for (auto const &x : ctx->classElement())
    {
        if (x->functionDefinition())
        {
            visit(x);
        }
    }

    currentPackage->currentModule->currentClass = nullptr;
    return nullptr;
}

any BalanceVisitor::visitClassInitializerExpression(BalanceParser::ClassInitializerExpressionContext *ctx)
{
    std::string text = ctx->getText();
    std::string className = ctx->classInitializer()->IDENTIFIER()->getText();

    BalanceClass *type;
    if (currentPackage->currentModule->classes.find(className) != currentPackage->currentModule->classes.end()) {
        // Class is in current module
        type = currentPackage->currentModule->classes[className];
    } else if (currentPackage->currentModule->importedClasses.find(className) != currentPackage->currentModule->importedClasses.end()) {
        // Class is imported
        type = currentPackage->currentModule->importedClasses[className];
    } else {
        // TODO: throw error
    }

    type = currentPackage->currentModule->classes[className];
    AllocaInst *alloca = currentPackage->currentModule->builder->CreateAlloca(type->structType);
    ArrayRef<Value *> argumentsReference{alloca};
    currentPackage->currentModule->builder->CreateCall(type->constructor, argumentsReference);
    return (Value *)alloca;
}

any BalanceVisitor::visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx)
{
    std::string text = ctx->getText();

    any anyValMember = visit(ctx->member);
    llvm::Value *valueMember = anyToValue(anyValMember);

    // Store this value, so that visiting the access will use it as context
    accessedValue = valueMember;
    any anyValAccess = visit(ctx->access);
    llvm::Value *value = anyToValue(anyValAccess);
    accessedValue = nullptr;
    return value;
}

any BalanceVisitor::visitWhileStatement(BalanceParser::WhileStatementContext *ctx)
{
    any text = ctx->getText();

    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();

    // Set up the 3 blocks we need: condition, loop-block and merge
    BasicBlock *condBlock = BasicBlock::Create(*currentPackage->currentModule->context, "loopcond", function);
    BasicBlock *loopBlock = BasicBlock::Create(*currentPackage->currentModule->context, "loop", function);
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->currentModule->context, "afterloop", function);

    // Jump to condition
    currentPackage->currentModule->builder->CreateBr(condBlock);
    currentPackage->currentModule->builder->SetInsertPoint(condBlock);

    // while condition, e.g. (i < 5)
    any expressionResult = visit(ctx->expression());
    llvm::Value *expression = anyToValue(expressionResult);

    // Create the condition - if expression is true, jump to loop block, else jump to after loop block
    currentPackage->currentModule->builder->CreateCondBr(expression, loopBlock, mergeBlock);

    // Set insert point to the loop-block so we can populate it
    currentPackage->currentModule->builder->SetInsertPoint(loopBlock);
    currentPackage->currentModule->currentScope = new ScopeBlock(loopBlock, currentPackage->currentModule->currentScope);

    // Visit the while-block statements
    visit(ctx->ifBlock());

    currentPackage->currentModule->currentScope = new ScopeBlock(condBlock, currentPackage->currentModule->currentScope->parent);
    // At the end of while-block, jump back to the condition which may jump to mergeBlock or reiterate
    currentPackage->currentModule->builder->CreateBr(condBlock);

    // Make sure new code is added to "block" after while statement
    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    currentPackage->currentModule->currentScope = new ScopeBlock(mergeBlock, currentPackage->currentModule->currentScope->parent); // TODO: Store scope in beginning of this function, and restore here?
    return any();
}

any BalanceVisitor::visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx)
{
    std::string text = ctx->getText();

    any anyValMember = visit(ctx->member);
    llvm::Value *valueMember = anyToValue(anyValMember);
    any anyValIndex = visit(ctx->index);
    llvm::Value *valueIndex = anyToValue(anyValIndex); // Should return i32 for now
    any anyVal = visit(ctx->value);
    llvm::Value *value = anyToValue(anyVal);

    auto zero = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, 0, true));
    auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember, {zero, valueIndex});
    currentPackage->currentModule->builder->CreateStore(value, ptr);

    return any();
}

any BalanceVisitor::visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx)
{
    std::string text = ctx->getText();
    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();

    any anyValMember = visit(ctx->member);
    llvm::Value *valueMember = anyToValue(anyValMember);
    any anyValIndex = visit(ctx->index);
    llvm::Value *valueIndex = anyToValue(anyValIndex); // Should return i32 for now

    auto zero = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, 0, true));
    auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember, {zero, valueIndex});
    return (Value *)currentPackage->currentModule->builder->CreateLoad(ptr);
}

any BalanceVisitor::visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx)
{
    std::string text = ctx->getText();
    vector<llvm::Value *> values;
    for (BalanceParser::ExpressionContext *expression : ctx->listElements()->expression())
    {
        any expressionResult = visit(expression);
        llvm::Value *value = anyToValue(expressionResult);
        values.push_back(value);
    }

    Type *type = values[0]->getType();
    auto arrayType = llvm::ArrayType::get(type, values.size());

    AllocaInst *alloca = currentPackage->currentModule->builder->CreateAlloca(arrayType);
    auto zero = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, 0, true));
    for (int i = 0; i < values.size(); i++)
    {
        auto index = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, i, true));
        auto ptr = currentPackage->currentModule->builder->CreateGEP(alloca, {zero, index});
        currentPackage->currentModule->builder->CreateStore(values[i], ptr);
    }
    return (Value *)alloca;
}

any BalanceVisitor::visitIfStatement(BalanceParser::IfStatementContext *ctx)
{
    std::string text = ctx->getText();
    ScopeBlock *scope = currentPackage->currentModule->currentScope;
    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();

    any expressionResult = visit(ctx->expression());
    llvm::Value *expression = anyToValue(expressionResult);
    BasicBlock *thenBlock = BasicBlock::Create(*currentPackage->currentModule->context, "then", function);
    BasicBlock *elseBlock = BasicBlock::Create(*currentPackage->currentModule->context, "else");
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->currentModule->context, "ifcont");
    currentPackage->currentModule->builder->CreateCondBr(expression, thenBlock, elseBlock);

    currentPackage->currentModule->builder->SetInsertPoint(thenBlock);
    currentPackage->currentModule->currentScope = new ScopeBlock(thenBlock, scope);
    visit(ctx->ifblock);
    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    thenBlock = currentPackage->currentModule->builder->GetInsertBlock();
    function->getBasicBlockList().push_back(elseBlock);
    currentPackage->currentModule->builder->SetInsertPoint(elseBlock);
    currentPackage->currentModule->currentScope = new ScopeBlock(elseBlock, scope);
    if (ctx->elseblock)
    {
        visit(ctx->elseblock);
    }
    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    elseBlock = currentPackage->currentModule->builder->GetInsertBlock();
    function->getBasicBlockList().push_back(mergeBlock);
    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    currentPackage->currentModule->currentScope = scope;
    return any();
}

any BalanceVisitor::visitArgument(BalanceParser::ArgumentContext *ctx)
{
    std::string text = ctx->getText();
    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();
    // if (ctx->expression())
    // {
    //     return visit(ctx->expression());
        // // Argument is a variable
        // std::string variableName = ctx->IDENTIFIER()->getText();
        // llvm::Value *val = currentPackage->currentModule->getValue(variableName);

        // Type *type = val->getType();

        // if (isa<PointerType>(val->getType()))
        // {
        //     llvm::Value *load = currentPackage->currentModule->builder->CreateLoad(val, variableName);
        //     return load;
        // }

        // return (Value *)val;
    // }
    return visitChildren(ctx);
}

any BalanceVisitor::visitVariableExpression(BalanceParser::VariableExpressionContext *ctx)
{
    std::string variableName = ctx->variable()->IDENTIFIER()->getText();
    Value *val = currentPackage->currentModule->getValue(variableName);
    if (val == nullptr && currentPackage->currentModule->currentClass != nullptr)
    {
        // TODO: Check if variableName is in currentClass->properties
        int intIndex = currentPackage->currentModule->currentClass->properties[variableName]->index;
        auto zero = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, intIndex, true));
        Value *thisValue = currentPackage->currentModule->getValue("this");
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue, {zero, index});
        return (Value *)currentPackage->currentModule->builder->CreateLoad(ptr);
    }

    if (val->getType()->isPointerTy() && !val->getType()->getPointerElementType()->isStructTy())
    {
        return (Value *)currentPackage->currentModule->builder->CreateLoad(val);
    }
    return val;
}

any BalanceVisitor::visitNewAssignment(BalanceParser::NewAssignmentContext *ctx)
{
    std::string text = ctx->getText();
    std::string variableName = ctx->IDENTIFIER()->getText();
    any expressionResult = visit(ctx->expression());
    Value *value = anyToValue(expressionResult);
    Value *alloca = currentPackage->currentModule->builder->CreateAlloca(value->getType(), nullptr);
    Value *store = currentPackage->currentModule->builder->CreateStore(value, alloca);
    currentPackage->currentModule->setValue(variableName, alloca);
    return alloca;
}

any BalanceVisitor::visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx)
{
    std::string text = ctx->getText();
    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();
    std::string variableName = ctx->IDENTIFIER()->getText();
    any expressionResult = visit(ctx->expression());
    llvm::Value *value = anyToValue(expressionResult);

    Value *variable = currentPackage->currentModule->getValue(variableName);
    if (variable == nullptr && currentPackage->currentModule->currentClass != nullptr)
    {
        // TODO: Check if variableName is in currentClass->properties
        int intIndex = currentPackage->currentModule->currentClass->properties[variableName]->index;
        auto zero = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, intIndex, true));
        Value *thisValue = currentPackage->currentModule->getValue("this");
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue, {zero, index});
        return (Value *)currentPackage->currentModule->builder->CreateStore(value, ptr);
    }

    return (Value *)currentPackage->currentModule->builder->CreateStore(value, variable);
}

any BalanceVisitor::visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx)
{
    any lhs = visit(ctx->lhs);
    any rhs = visit(ctx->rhs);

    llvm::Value *lhsVal = anyToValue(lhs);
    llvm::Value *rhsVal = anyToValue(rhs);

    if (ctx->LT())
    {
        return (Value *)currentPackage->currentModule->builder->CreateICmpSLT(lhsVal, rhsVal, "lttmp");
    }
    else if (ctx->GT())
    {
        return (Value *)currentPackage->currentModule->builder->CreateICmpSGT(lhsVal, rhsVal, "gttmp");
    }
    else if (ctx->OP_LE())
    {
        return (Value *)currentPackage->currentModule->builder->CreateICmpSLE(lhsVal, rhsVal, "letmp");
    }
    else if (ctx->OP_GE())
    {
        return (Value *)currentPackage->currentModule->builder->CreateICmpSGE(lhsVal, rhsVal, "getmp");
    }

    return any();
}

any BalanceVisitor::visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx)
{
    any lhs = visit(ctx->lhs);
    any rhs = visit(ctx->rhs);

    llvm::Value *lhsVal = anyToValue(lhs);
    llvm::Value *rhsVal = anyToValue(rhs);

    // If either operand is float, cast other to float (consolidate this whole thing)
    if (lhsVal->getType()->isFloatingPointTy() || rhsVal->getType()->isFloatingPointTy())
    {
        if (!lhsVal->getType()->isFloatingPointTy())
        {
            if (lhsVal->getType()->isIntegerTy())
            {
                int width = lhsVal->getType()->getIntegerBitWidth();
                if (width == 1)
                {
                    // bool
                    lhsVal = currentPackage->currentModule->builder->CreateUIToFP(lhsVal, getBuiltinType("Double"));
                }
                else if (width == 32)
                {
                    // int32
                    lhsVal = currentPackage->currentModule->builder->CreateSIToFP(lhsVal, getBuiltinType("Double"));
                }
            }
        }
        if (!rhsVal->getType()->isFloatingPointTy())
        {
            if (rhsVal->getType()->isIntegerTy())
            {
                int width = rhsVal->getType()->getIntegerBitWidth();
                if (width == 1)
                {
                    // bool
                    rhsVal = currentPackage->currentModule->builder->CreateUIToFP(rhsVal, getBuiltinType("Double"));
                }
                else if (width == 32)
                {
                    // int32
                    rhsVal = currentPackage->currentModule->builder->CreateSIToFP(rhsVal, getBuiltinType("Double"));
                }
            }
        }
    }

    if (ctx->STAR())
    {
        // Just check one of them, since both will be floats or none.
        if (lhsVal->getType()->isFloatingPointTy())
        {
            return (Value *)currentPackage->currentModule->builder->CreateFMul(lhsVal, rhsVal);
        }
        return (Value *)currentPackage->currentModule->builder->CreateMul(lhsVal, rhsVal);
    }
    else
    {
        if (lhsVal->getType()->isFloatingPointTy())
        {
            return (Value *)currentPackage->currentModule->builder->CreateFDiv(lhsVal, rhsVal);
        }
        return (Value *)currentPackage->currentModule->builder->CreateSDiv(lhsVal, rhsVal);
    }

    return any();
}

any BalanceVisitor::visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx)
{
    any lhs = visit(ctx->lhs);
    any rhs = visit(ctx->rhs);

    llvm::Value *lhsVal = anyToValue(lhs);
    llvm::Value *rhsVal = anyToValue(rhs);

    // If either operand is float, cast other to float (consolidate this whole thing)
    if (lhsVal->getType()->isFloatingPointTy() || rhsVal->getType()->isFloatingPointTy())
    {
        if (!lhsVal->getType()->isFloatingPointTy())
        {
            if (lhsVal->getType()->isIntegerTy())
            {
                int width = lhsVal->getType()->getIntegerBitWidth();
                if (width == 1)
                {
                    // bool
                    lhsVal = currentPackage->currentModule->builder->CreateUIToFP(lhsVal, getBuiltinType("Double"));
                }
                else if (width == 32)
                {
                    // int32
                    lhsVal = currentPackage->currentModule->builder->CreateSIToFP(lhsVal, getBuiltinType("Double"));
                }
            }
        }
        if (!rhsVal->getType()->isFloatingPointTy())
        {
            if (rhsVal->getType()->isIntegerTy())
            {
                int width = rhsVal->getType()->getIntegerBitWidth();
                if (width == 1)
                {
                    // bool
                    rhsVal = currentPackage->currentModule->builder->CreateUIToFP(rhsVal, getBuiltinType("Double"));
                }
                else if (width == 32)
                {
                    // int32
                    rhsVal = currentPackage->currentModule->builder->CreateSIToFP(rhsVal, getBuiltinType("Double"));
                }
            }
        }
    }

    if (ctx->PLUS())
    {
        // Just check one of them, since both will be floats or none.
        if (lhsVal->getType()->isFloatingPointTy())
        {
            return (Value *)currentPackage->currentModule->builder->CreateFAdd(lhsVal, rhsVal);
        }
        return (Value *)currentPackage->currentModule->builder->CreateAdd(lhsVal, rhsVal);
    }
    else
    {
        if (lhsVal->getType()->isFloatingPointTy())
        {
            return (Value *)currentPackage->currentModule->builder->CreateFSub(lhsVal, rhsVal);
        }
        return (Value *)currentPackage->currentModule->builder->CreateSub(lhsVal, rhsVal);
    }

    return any();
}

any BalanceVisitor::visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx)
{
    std::string value = ctx->DECIMAL_INTEGER()->getText();
    Type *i32_type = IntegerType::getInt32Ty(*currentPackage->currentModule->context);
    int intValue = stoi(value);
    return (Value *)ConstantInt::get(i32_type, intValue, true);
}

any BalanceVisitor::visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx)
{
    Type *type = getBuiltinType("Bool");
    if (ctx->TRUE())
    {
        return (Value *)ConstantInt::get(type, 1, true);
    }
    return (Value *)ConstantInt::get(type, 0, true);
}

any BalanceVisitor::visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx)
{
    std::string value = ctx->DOUBLE()->getText();
    Type *type = getBuiltinType("Double");
    double doubleValue = stod(value);
    return (Value *)ConstantFP::get(type, doubleValue);
}

any BalanceVisitor::visitStringLiteral(BalanceParser::StringLiteralContext *ctx)
{
    // TODO: is CreateGlobalStringPtr the best solution?
    std::string text = ctx->STRING()->getText();
    return (Value *)currentPackage->currentModule->builder->CreateGlobalStringPtr(text);
}

any BalanceVisitor::visitFunctionCall(BalanceParser::FunctionCallContext *ctx)
{
    std::string text = ctx->getText();
    std::string functionName = ctx->IDENTIFIER()->getText();
    // TODO: Make a search through scope, parent scopes, builtins etc.
    if (accessedValue == nullptr && functionName == "print")
    {
        FunctionCallee printfFunc = currentPackage->currentModule->module->getFunction("printf");   // TODO: Move this to separate module
        vector<Value *> functionArguments;
        for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument())
        {
            any anyVal = visit(argument);
            llvm::Value *value = anyToValue(anyVal);
            if (PointerType *PT = dyn_cast<PointerType>(value->getType()))
            {
                if (PT == Type::getInt8PtrTy(*currentPackage->currentModule->context, PT->getPointerAddressSpace()))
                {
                    auto args = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "%s\n", "args"), value};
                    return (Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, args);
                }
                else if (PT->getElementType()->isArrayTy())
                {
                    auto zero = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, 0, true));
                    auto argsBefore = ArrayRef<llvm::Value *>{geti8StrVal(*currentPackage->currentModule->module, "[", "args")};
                    (llvm::Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, argsBefore);
                    int numElements = PT->getElementType()->getArrayNumElements();
                    // TODO: Optimize this, so we generate ONE string e.g. "%d, %d, %d, %d\n"
                    // Also, make a function that does this, which takes value, type and whether to linebreak?
                    for (int i = 0; i < numElements; i++)
                    {
                        auto index = ConstantInt::get(*currentPackage->currentModule->context, llvm::APInt(32, i, true));
                        auto ptr = currentPackage->currentModule->builder->CreateGEP(value, {zero, index});
                        llvm::Value *valueAtIndex = (Value *)currentPackage->currentModule->builder->CreateLoad(ptr);
                        if (i < numElements - 1)
                        {
                            auto args = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "%d, ", "args"), valueAtIndex};
                            (Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, args);
                        }
                        else
                        {
                            auto args = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "%d", "args"), valueAtIndex};
                            (Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, args);
                        }
                    }
                    auto argsAfter = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "]\n", "args")};
                    (Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, argsAfter);

                    return any();
                }
            }
            else if (IntegerType *IT = dyn_cast<IntegerType>(value->getType()))
            {
                int width = value->getType()->getIntegerBitWidth();
                if (width == 1)
                {
                    // Figure out how to print 'true' or 'false' when we know runtime value
                    auto args = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "%d\n", "args"), value};
                    return (Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, args);
                }
                else
                {
                    auto args = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "%d\n", "args"), value};
                    return (Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, args);
                }
            }
            else if (value->getType()->isFloatingPointTy())
            {
                auto args = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "%g\n", "args"), value};
                return (Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, args);
            }
            break;
        }
    }
    else
    {
        if (accessedValue != nullptr)
        {
            // Means we're invoking function on an element
            Type *elementType = accessedValue->getType();
            vector<Value *> functionArguments;

            if (PointerType *PT = dyn_cast<PointerType>(accessedValue->getType()))
            {
                // TODO: Check that it is actually struct
                std::string className = PT->getElementType()->getStructName().str();

                // Add "this" as first argument
                functionArguments.push_back(accessedValue);

                for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument())
                {
                    any anyVal = visit(argument);
                    llvm::Value *castVal = anyToValue(anyVal);
                    functionArguments.push_back(castVal);
                }

                BalanceClass *bClass = currentPackage->currentModule->getClass(className);
                if (bClass == nullptr) {
                    bClass = currentPackage->currentModule->getImportedClass(className);
                    if (bClass == nullptr) {
                        // TODO: Throw error.
                    }
                }
                Function *function = bClass->methods[functionName]->function;
                FunctionType *functionType = function->getFunctionType();

                ArrayRef<Value *> argumentsReference(functionArguments);
                return (Value *)currentPackage->currentModule->builder->CreateCall(functionType, (Value *)function, argumentsReference);
            } else {
                // TODO: Handle
            }
        }

        // Check if function is a variable (lambda e.g.)
        llvm::Value *val = currentPackage->currentModule->getValue(functionName);
        if (val)
        {
            auto ty = val->getType();
            if (val->getType()->getPointerElementType()->isFunctionTy())
            {
                vector<Value *> functionArguments;

                for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument())
                {
                    any anyVal = visit(argument);
                    llvm::Value *castVal = anyToValue(anyVal);
                    functionArguments.push_back(castVal);
                }

                ArrayRef<Value *> argumentsReference(functionArguments);
                return (Value *)currentPackage->currentModule->builder->CreateCall(FunctionType::get(val->getType(), false), val, argumentsReference);
            }
            else if (val->getType()->getPointerElementType()->isPointerTy())
            {
                Value *loaded = currentPackage->currentModule->builder->CreateLoad(val);
                vector<Value *> functionArguments;

                for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument())
                {
                    any anyVal = visit(argument);
                    llvm::Value *castVal = anyToValue(anyVal);
                    functionArguments.push_back(castVal);
                }

                ArrayRef<Value *> argumentsReference(functionArguments);
                return (Value *)currentPackage->currentModule->builder->CreateCall(FunctionType::get(loaded->getType(), false), loaded, argumentsReference);
            }
        }
        else
        {
            BalanceFunction * bfunction = currentPackage->currentModule->getFunction(functionName);
            if (bfunction == nullptr) {
                bfunction = currentPackage->currentModule->getImportedFunction(functionName);
                if (bfunction == nullptr) {
                    // TODO: Throw error
                }
            }

            FunctionCallee function = bfunction->function;
            vector<Value *> functionArguments;

            for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument())
            {
                any anyVal = visit(argument);
                llvm::Value *castVal = anyToValue(anyVal);
                functionArguments.push_back(castVal);
            }

            ArrayRef<Value *> argumentsReference(functionArguments);
            return (Value *)currentPackage->currentModule->builder->CreateCall(function, argumentsReference);
        }
    }

    return any();
}

any BalanceVisitor::visitReturnStatement(BalanceParser::ReturnStatementContext *ctx)
{
    std::string text = ctx->getText();
    if (ctx->expression()->getText() == "None")
    {
        // handled in visitFunctionDefinition, since we will not hit this method on implicit 'return None'
    }
    else
    {
        any anyVal = visit(ctx->expression());
        llvm::Value *castVal = anyToValue(anyVal);
        currentPackage->currentModule->builder->CreateRet(castVal);
    }
    return any();
}

any BalanceVisitor::visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx)
{
    // https://stackoverflow.com/questions/54905211/how-to-implement-function-pointer-by-using-llvm-c-api
    std::string text = ctx->getText();
    vector<Type *> functionParameterTypes;
    vector<std::string> functionParameterNames;
    for (BalanceParser::ParameterContext *parameter : ctx->lambda()->parameterList()->parameter())
    {
        std::string parameterName = parameter->identifier->getText();
        functionParameterNames.push_back(parameterName);
        std::string typeString = parameter->type->getText();
        Type *type = getBuiltinType(typeString); // TODO: Handle unknown type
        functionParameterTypes.push_back(type);
    }

    // If we don't have a return type, assume none
    Type *returnType;
    if (ctx->lambda()->returnType())
    {
        std::string functionReturnTypeString = ctx->lambda()->returnType()->IDENTIFIER()->getText();
        returnType = getBuiltinType(functionReturnTypeString); // TODO: Handle unknown type
    }
    else
    {
        returnType = getBuiltinType("None");
    }

    ArrayRef<Type *> parametersReference(functionParameterTypes);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
    Function *function = Function::Create(functionType, Function::InternalLinkage, "", currentPackage->currentModule->module);

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    for (std::string parameterName : functionParameterNames)
    {
        llvm::Value *x = args++;
        x->setName(parameterName);
        currentPackage->currentModule->setValue(parameterName, x);
    }

    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->currentModule->context, "", function);
    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);
    ScopeBlock *scope = currentPackage->currentModule->currentScope;
    currentPackage->currentModule->currentScope = new ScopeBlock(functionBody, scope);

    visit(ctx->lambda()->functionBlock());

    if (returnType->isVoidTy())
    {
        currentPackage->currentModule->builder->CreateRetVoid();
    }

    bool hasError = verifyFunction(*function);
    if (hasError) {
        // TODO: Throw error and give more context
        std::cout << "Error verifying lambda expression: " << text << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        exit(1);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
    currentPackage->currentModule->currentScope = scope;

    // Create alloca so we can return it as expression
    llvm::Value *p = currentPackage->currentModule->builder->CreateAlloca(function->getType(), nullptr, "");
    currentPackage->currentModule->builder->CreateStore(function, p, false);
    return (llvm::Value *)currentPackage->currentModule->builder->CreateLoad(p);
}

any BalanceVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx)
{
    std::string functionName = ctx->IDENTIFIER()->getText();
    BalanceFunction * bfunction;

    if (currentPackage->currentModule->currentClass != nullptr) {
        bfunction = currentPackage->currentModule->currentClass->methods[functionName];
    } else {
        bfunction = currentPackage->currentModule->getFunction(functionName);
    }

    ScopeBlock *scope = currentPackage->currentModule->currentScope;
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->currentModule->context, functionName + "_body", bfunction->function);
    currentPackage->currentModule->currentScope = new ScopeBlock(functionBody, scope);

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    visit(ctx->functionBlock());

    if (bfunction->returnType->isVoidTy())
    {
        currentPackage->currentModule->builder->CreateRetVoid();
    }

    bool hasError = verifyFunction(*bfunction->function, &llvm::errs());
    if (hasError) {
        // TODO: Throw error
        std::cout << "Error verifying function: " << bfunction->name << std::endl;
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        exit(1);
    }

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
    currentPackage->currentModule->currentScope = scope;
    return nullptr;
}
