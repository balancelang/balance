#include "headers/Visitor.h"
#include "headers/Package.h"

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
using namespace std;

extern BalancePackage *currentPackage;
extern LLVMContext *context;
extern IRBuilder<> *builder;

// Used to store e.g. 'x' in 'x.toString()', so we know 'toString()' is attached to x.
Value *accessedValue;
extern BalanceModule *currentModule;
BalanceClass *currentClass = nullptr;

void LogError(string errorMessage)
{
    fprintf(stderr, "Error: %s\n", errorMessage.c_str());
}

Value *anyToValue(any anyVal)
{
    return any_cast<Value *>(anyVal);
}

Value *getValue(string variableName)
{
    ScopeBlock *scope = currentModule->currentScope;
    while (scope != nullptr)
    {
        Value *tryVal = scope->symbolTable[variableName];
        if (tryVal != nullptr)
        {
            return tryVal;
        }
        scope = scope->parent;
    }
    // LogError("Failed to find variable in scope or parent scopes: " + variableName); // TODO: Move this to caller, when checking if nullptr
    return nullptr;
}

void setValue(string variableName, Value *value)
{
    currentModule->currentScope->symbolTable[variableName] = value;
}

Type *
getBuiltinType(string typeString)
{
    if (typeString == "Int")
    {
        return Type::getInt32Ty(*context);
    }
    else if (typeString == "Bool")
    {
        return Type::getInt1Ty(*context);
    }
    else if (typeString == "String")
    {
        return Type::getInt8PtrTy(*context);
    }
    else if (typeString == "Double")
    {
        return Type::getDoubleTy(*context);
    }
    else if (typeString == "None")
    {
        return Type::getVoidTy(*context);
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
    string constructorName = classValue->getName().str() + "_constructor";
    vector<Type *> functionParameterTypes;

    // If we don't have a return type, assume none
    Type *returnType = getBuiltinType("None");

    ArrayRef<Type *> parametersReference{classValue->getPointerTo()};
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
    Function *function = Function::Create(functionType, Function::InternalLinkage, constructorName, currentModule->module);
    currentClass->constructor = function;

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    llvm::Value *thisValue = args++;
    thisValue->setName("this");

    BasicBlock *functionBody = BasicBlock::Create(*context, constructorName + "_body", function);
    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = builder->GetInsertBlock();
    builder->SetInsertPoint(functionBody);

    for (auto const &x : currentClass->properties)
    {
        Type *propertyType = x.second.type;

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
        // TODO: Handle String and nullable types

        int intIndex = x.second.index;
        auto zero = ConstantInt::get(*context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*context, llvm::APInt(32, intIndex, true));
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = builder->CreateGEP(structType, thisValue, {zero, index});
        builder->CreateStore(initialValue, ptr);
    }

    builder->CreateRetVoid();

    bool hasError = verifyFunction(*function);
    builder->SetInsertPoint(resumeBlock);
}

void finalizeClass()
{
    vector<Type *> propertyTypes;
    for (auto const &x : currentClass->properties)
    {
        propertyTypes.push_back(x.second.type);
    }
    ArrayRef<Type *> propertyTypesRef(propertyTypes);
    currentClass->structType->setBody(propertyTypesRef, false);
    currentClass->finalized = true;
}

any BalanceVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx)
{
    string text = ctx->getText();
    string className = ctx->className->getText();
    currentClass = currentModule->classes[className];

    StructType *structType = StructType::create(*context, className);
    currentClass->structType = structType;

    // We make sure to parse all classProperties first, so we can finalize class.
    for (auto const &x : ctx->classElement())
    {
        if (x->classProperty())
        {
            visit(x);
        }
    }

    finalizeClass();
    createDefaultConstructor(structType);

    for (auto const &x : ctx->classElement())
    {
        if (x->functionDefinition())
        {
            visit(x);
        }
    }

    currentClass = nullptr;
    return nullptr;
}

any BalanceVisitor::visitClassInitializerExpression(BalanceParser::ClassInitializerExpressionContext *ctx)
{
    string text = ctx->getText();
    string className = ctx->classInitializer()->IDENTIFIER()->getText();

    BalanceClass *type = currentModule->classes[className];
    AllocaInst *alloca = builder->CreateAlloca(type->structType);
    ArrayRef<Value *> argumentsReference{alloca};
    builder->CreateCall(type->constructor, argumentsReference);
    // return (Value *)builder->CreateLoad(alloca);
    return (Value *)alloca;
}

any BalanceVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx)
{
    string text = ctx->getText();
    string typeString = ctx->type->getText();
    string name = ctx->name->getText();

    Type *typeValue = getBuiltinType(typeString);
    if (typeValue == nullptr)
    {
        // TODO: Handle non builtin types (e.g. other classes)
    }

    int count = currentClass->properties.size();
    currentClass->properties[name] = {count, typeValue};
    return any();
}

any BalanceVisitor::visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx)
{
    string text = ctx->getText();

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

    Function *function = builder->GetInsertBlock()->getParent();

    // Set up the 3 blocks we need: condition, loop-block and merge
    BasicBlock *condBlock = BasicBlock::Create(*context, "loopcond", function);
    BasicBlock *loopBlock = BasicBlock::Create(*context, "loop", function);
    BasicBlock *mergeBlock = BasicBlock::Create(*context, "afterloop", function);

    // Jump to condition
    builder->CreateBr(condBlock);
    builder->SetInsertPoint(condBlock);

    // while condition, e.g. (i < 5)
    any expressionResult = visit(ctx->expression());
    llvm::Value *expression = anyToValue(expressionResult);

    // Create the condition - if expression is true, jump to loop block, else jump to after loop block
    builder->CreateCondBr(expression, loopBlock, mergeBlock);

    // Set insert point to the loop-block so we can populate it
    builder->SetInsertPoint(loopBlock);
    currentModule->currentScope = new ScopeBlock(loopBlock, currentModule->currentScope);

    // Visit the while-block statements
    visit(ctx->ifBlock());

    currentModule->currentScope = new ScopeBlock(condBlock, currentModule->currentScope->parent);
    // At the end of while-block, jump back to the condition which may jump to mergeBlock or reiterate
    builder->CreateBr(condBlock);

    // Make sure new code is added to "block" after while statement
    builder->SetInsertPoint(mergeBlock);
    currentModule->currentScope = new ScopeBlock(mergeBlock, currentModule->currentScope->parent); // TODO: Store scope in beginning of this function, and restore here?
    return any();
}

any BalanceVisitor::visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx)
{
    string text = ctx->getText();

    any anyValMember = visit(ctx->member);
    llvm::Value *valueMember = anyToValue(anyValMember);
    any anyValIndex = visit(ctx->index);
    llvm::Value *valueIndex = anyToValue(anyValIndex); // Should return i32 for now
    any anyVal = visit(ctx->value);
    llvm::Value *value = anyToValue(anyVal);

    auto zero = ConstantInt::get(*context, llvm::APInt(32, 0, true));
    auto ptr = builder->CreateGEP(valueMember, {zero, valueIndex});
    builder->CreateStore(value, ptr);

    return any();
}

any BalanceVisitor::visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx)
{
    string text = ctx->getText();
    Function *function = builder->GetInsertBlock()->getParent();

    any anyValMember = visit(ctx->member);
    llvm::Value *valueMember = anyToValue(anyValMember);
    any anyValIndex = visit(ctx->index);
    llvm::Value *valueIndex = anyToValue(anyValIndex); // Should return i32 for now

    auto zero = ConstantInt::get(*context, llvm::APInt(32, 0, true));
    auto ptr = builder->CreateGEP(valueMember, {zero, valueIndex});
    return (Value *)builder->CreateLoad(ptr);
}

any BalanceVisitor::visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx)
{
    string text = ctx->getText();
    vector<llvm::Value *> values;
    for (BalanceParser::ExpressionContext *expression : ctx->listElements()->expression())
    {
        any expressionResult = visit(expression);
        llvm::Value *value = anyToValue(expressionResult);
        values.push_back(value);
    }

    Type *type = values[0]->getType();
    auto arrayType = llvm::ArrayType::get(type, values.size());

    AllocaInst *alloca = builder->CreateAlloca(arrayType);
    auto zero = ConstantInt::get(*context, llvm::APInt(32, 0, true));
    for (int i = 0; i < values.size(); i++)
    {
        auto index = ConstantInt::get(*context, llvm::APInt(32, i, true));
        auto ptr = builder->CreateGEP(alloca, {zero, index});
        builder->CreateStore(values[i], ptr);
    }
    return (Value *)alloca;
}

any BalanceVisitor::visitIfStatement(BalanceParser::IfStatementContext *ctx)
{
    string text = ctx->getText();
    ScopeBlock *scope = currentModule->currentScope;
    Function *function = builder->GetInsertBlock()->getParent();

    any expressionResult = visit(ctx->expression());
    llvm::Value *expression = anyToValue(expressionResult);
    BasicBlock *thenBlock = BasicBlock::Create(*context, "then", function);
    BasicBlock *elseBlock = BasicBlock::Create(*context, "else");
    BasicBlock *mergeBlock = BasicBlock::Create(*context, "ifcont");
    builder->CreateCondBr(expression, thenBlock, elseBlock);

    builder->SetInsertPoint(thenBlock);
    currentModule->currentScope = new ScopeBlock(thenBlock, scope);
    visit(ctx->ifblock);
    builder->CreateBr(mergeBlock);
    thenBlock = builder->GetInsertBlock();
    function->getBasicBlockList().push_back(elseBlock);
    builder->SetInsertPoint(elseBlock);
    currentModule->currentScope = new ScopeBlock(elseBlock, scope);
    if (ctx->elseblock)
    {
        visit(ctx->elseblock);
    }
    builder->CreateBr(mergeBlock);
    elseBlock = builder->GetInsertBlock();
    function->getBasicBlockList().push_back(mergeBlock);
    builder->SetInsertPoint(mergeBlock);
    currentModule->currentScope = scope;
    return any();
}

any BalanceVisitor::visitArgument(BalanceParser::ArgumentContext *ctx)
{
    string text = ctx->getText();
    Function *function = builder->GetInsertBlock()->getParent();
    if (ctx->IDENTIFIER())
    {
        // Argument is a variable
        string variableName = ctx->IDENTIFIER()->getText();
        llvm::Value *val = getValue(variableName);

        Type *type = val->getType();

        if (isa<PointerType>(val->getType()))
        {
            llvm::Value *load = builder->CreateLoad(val, variableName);
            return load;
        }

        return (Value *)val;
    }
    return visitChildren(ctx);
}

any BalanceVisitor::visitVariableExpression(BalanceParser::VariableExpressionContext *ctx)
{
    string variableName = ctx->variable()->IDENTIFIER()->getText();
    Value *val = getValue(variableName);
    if (val == nullptr && currentClass != nullptr)
    {
        // TODO: Check if variableName is in currentClass->properties
        int intIndex = currentClass->properties[variableName].index;
        auto zero = ConstantInt::get(*context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*context, llvm::APInt(32, intIndex, true));
        Value *thisValue = getValue("this");
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = builder->CreateGEP(structType, thisValue, {zero, index});
        return (Value *)builder->CreateLoad(ptr);
    }

    if (val->getType()->isPointerTy())
    {
        return (Value *)builder->CreateLoad(val);
    }
    return val;
}

any BalanceVisitor::visitNewAssignment(BalanceParser::NewAssignmentContext *ctx)
{
    string text = ctx->getText();
    string variableName = ctx->IDENTIFIER()->getText();
    any expressionResult = visit(ctx->expression());
    Value *value = anyToValue(expressionResult);
    Value *alloca = builder->CreateAlloca(value->getType(), nullptr);
    Value *store = builder->CreateStore(value, alloca);
    setValue(variableName, alloca);
    return alloca;
}

any BalanceVisitor::visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx)
{
    string text = ctx->getText();
    Function *function = builder->GetInsertBlock()->getParent();
    string variableName = ctx->IDENTIFIER()->getText();
    any expressionResult = visit(ctx->expression());
    llvm::Value *value = anyToValue(expressionResult);

    Value *variable = getValue(variableName);
    if (variable == nullptr && currentClass != nullptr)
    {
        // TODO: Check if variableName is in currentClass->properties
        int intIndex = currentClass->properties[variableName].index;
        auto zero = ConstantInt::get(*context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*context, llvm::APInt(32, intIndex, true));
        Value *thisValue = getValue("this");
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = builder->CreateGEP(structType, thisValue, {zero, index});
        return (Value *)builder->CreateStore(value, ptr);
    }

    return (Value *)builder->CreateStore(value, variable);
}

any BalanceVisitor::visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx)
{
    any lhs = visit(ctx->lhs);
    any rhs = visit(ctx->rhs);

    llvm::Value *lhsVal = anyToValue(lhs);
    llvm::Value *rhsVal = anyToValue(rhs);

    if (ctx->LT())
    {
        return (Value *)builder->CreateICmpSLT(lhsVal, rhsVal, "lttmp");
    }
    else if (ctx->GT())
    {
        return (Value *)builder->CreateICmpSGT(lhsVal, rhsVal, "gttmp");
    }
    else if (ctx->OP_LE())
    {
        return (Value *)builder->CreateICmpSLE(lhsVal, rhsVal, "letmp");
    }
    else if (ctx->OP_GE())
    {
        return (Value *)builder->CreateICmpSGE(lhsVal, rhsVal, "getmp");
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
                    lhsVal = builder->CreateUIToFP(lhsVal, getBuiltinType("Double"));
                }
                else if (width == 32)
                {
                    // int32
                    lhsVal = builder->CreateSIToFP(lhsVal, getBuiltinType("Double"));
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
                    rhsVal = builder->CreateUIToFP(rhsVal, getBuiltinType("Double"));
                }
                else if (width == 32)
                {
                    // int32
                    rhsVal = builder->CreateSIToFP(rhsVal, getBuiltinType("Double"));
                }
            }
        }
    }

    if (ctx->STAR())
    {
        // Just check one of them, since both will be floats or none.
        if (lhsVal->getType()->isFloatingPointTy())
        {
            return (Value *)builder->CreateFMul(lhsVal, rhsVal);
        }
        return (Value *)builder->CreateMul(lhsVal, rhsVal);
    }
    else
    {
        if (lhsVal->getType()->isFloatingPointTy())
        {
            return (Value *)builder->CreateFDiv(lhsVal, rhsVal);
        }
        return (Value *)builder->CreateSDiv(lhsVal, rhsVal);
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
                    lhsVal = builder->CreateUIToFP(lhsVal, getBuiltinType("Double"));
                }
                else if (width == 32)
                {
                    // int32
                    lhsVal = builder->CreateSIToFP(lhsVal, getBuiltinType("Double"));
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
                    rhsVal = builder->CreateUIToFP(rhsVal, getBuiltinType("Double"));
                }
                else if (width == 32)
                {
                    // int32
                    rhsVal = builder->CreateSIToFP(rhsVal, getBuiltinType("Double"));
                }
            }
        }
    }

    if (ctx->PLUS())
    {
        // Just check one of them, since both will be floats or none.
        if (lhsVal->getType()->isFloatingPointTy())
        {
            return (Value *)builder->CreateFAdd(lhsVal, rhsVal);
        }
        return (Value *)builder->CreateAdd(lhsVal, rhsVal);
    }
    else
    {
        if (lhsVal->getType()->isFloatingPointTy())
        {
            return (Value *)builder->CreateFSub(lhsVal, rhsVal);
        }
        return (Value *)builder->CreateSub(lhsVal, rhsVal);
    }

    return any();
}

any BalanceVisitor::visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx)
{
    string value = ctx->DECIMAL_INTEGER()->getText();
    Type *i32_type = IntegerType::getInt32Ty(*context);
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
    string value = ctx->DOUBLE()->getText();
    Type *type = getBuiltinType("Double");
    double doubleValue = stod(value);
    return (Value *)ConstantFP::get(type, doubleValue);
}

any BalanceVisitor::visitStringLiteral(BalanceParser::StringLiteralContext *ctx)
{
    // TODO: is CreateGlobalStringPtr the best solution?
    string text = ctx->STRING()->getText();
    return (Value *)builder->CreateGlobalStringPtr(text);
}

any BalanceVisitor::visitFunctionCall(BalanceParser::FunctionCallContext *ctx)
{
    string text = ctx->getText();
    string functionName = ctx->IDENTIFIER()->getText();
    // TODO: Make a search through scope, parent scopes, builtins etc.
    if (accessedValue == nullptr && functionName == "print")
    {
        FunctionCallee printfFunc = currentModule->module->getFunction("printf");   // TODO: Move this to separate module
        vector<Value *> functionArguments;
        for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument())
        {
            any anyVal = visit(argument);
            llvm::Value *value = anyToValue(anyVal);
            if (PointerType *PT = dyn_cast<PointerType>(value->getType()))
            {
                if (PT == Type::getInt8PtrTy(*context, PT->getPointerAddressSpace()))
                {
                    auto args = ArrayRef<Value *>{geti8StrVal(*currentModule->module, "%s\n", "args"), value};
                    return (Value *)builder->CreateCall(printfFunc, args);
                }
                else if (PT->getElementType()->isArrayTy())
                {
                    auto zero = ConstantInt::get(*context, llvm::APInt(32, 0, true));
                    auto argsBefore = ArrayRef<Value *>{geti8StrVal(*currentModule->module, "[", "args")};
                    (Value *)builder->CreateCall(printfFunc, argsBefore);
                    int numElements = PT->getElementType()->getArrayNumElements();
                    // TODO: Optimize this, so we generate ONE string e.g. "%d, %d, %d, %d\n"
                    // Also, make a function that does this, which takes value, type and whether to linebreak?
                    for (int i = 0; i < numElements; i++)
                    {
                        auto index = ConstantInt::get(*context, llvm::APInt(32, i, true));
                        auto ptr = builder->CreateGEP(value, {zero, index});
                        llvm::Value *valueAtIndex = (Value *)builder->CreateLoad(ptr);
                        if (i < numElements - 1)
                        {
                            auto args = ArrayRef<Value *>{geti8StrVal(*currentModule->module, "%d, ", "args"), valueAtIndex};
                            (Value *)builder->CreateCall(printfFunc, args);
                        }
                        else
                        {
                            auto args = ArrayRef<Value *>{geti8StrVal(*currentModule->module, "%d", "args"), valueAtIndex};
                            (Value *)builder->CreateCall(printfFunc, args);
                        }
                    }
                    auto argsAfter = ArrayRef<Value *>{geti8StrVal(*currentModule->module, "]\n", "args")};
                    (Value *)builder->CreateCall(printfFunc, argsAfter);

                    return any();
                }
            }
            else if (IntegerType *IT = dyn_cast<IntegerType>(value->getType()))
            {
                int width = value->getType()->getIntegerBitWidth();
                if (width == 1)
                {
                    // Figure out how to print 'true' or 'false' when we know runtime value
                    auto args = ArrayRef<Value *>{geti8StrVal(*currentModule->module, "%d\n", "args"), value};
                    return (Value *)builder->CreateCall(printfFunc, args);
                }
                else
                {
                    auto args = ArrayRef<Value *>{geti8StrVal(*currentModule->module, "%d\n", "args"), value};
                    return (Value *)builder->CreateCall(printfFunc, args);
                }
            }
            else if (value->getType()->isFloatingPointTy())
            {
                auto args = ArrayRef<Value *>{geti8StrVal(*currentModule->module, "%g\n", "args"), value};
                return (Value *)builder->CreateCall(printfFunc, args);
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
                string className = PT->getElementType()->getStructName().str();

                functionArguments.push_back(accessedValue);

                for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument())
                {
                    any anyVal = visit(argument);
                    llvm::Value *castVal = anyToValue(anyVal);
                    functionArguments.push_back(castVal);
                }

                BalanceClass *bClass = currentModule->classes[className];
                Function *function = bClass->methods[functionName];
                FunctionType *functionType = function->getFunctionType();

                ArrayRef<Value *> argumentsReference(functionArguments);
                return (Value *)builder->CreateCall(functionType, (Value *)function, argumentsReference);
            }
            else
            {
                // TODO: Handle invoking functions on non-structs
            }
        }

        // Check if function is a variable (lambda e.g.)
        llvm::Value *val = getValue(functionName);
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
                return (Value *)builder->CreateCall(FunctionType::get(val->getType(), false), val, argumentsReference);
            }
            else if (val->getType()->getPointerElementType()->isPointerTy())
            {
                Value *loaded = builder->CreateLoad(val);
                vector<Value *> functionArguments;

                for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument())
                {
                    any anyVal = visit(argument);
                    llvm::Value *castVal = anyToValue(anyVal);
                    functionArguments.push_back(castVal);
                }

                ArrayRef<Value *> argumentsReference(functionArguments);
                return (Value *)builder->CreateCall(FunctionType::get(loaded->getType(), false), loaded, argumentsReference);
            }
        }
        else
        {
            // FunctionCallee function = currentModule->module->getFunction(functionName);
            BalanceFunction * bfunction = currentModule->functions[functionName];
            FunctionCallee function = bfunction->function;
            vector<Value *> functionArguments;

            for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument())
            {
                any anyVal = visit(argument);
                llvm::Value *castVal = anyToValue(anyVal);
                functionArguments.push_back(castVal);
            }

            ArrayRef<Value *> argumentsReference(functionArguments);
            return (Value *)builder->CreateCall(function, argumentsReference);
        }
    }

    return any();
}

any BalanceVisitor::visitReturnStatement(BalanceParser::ReturnStatementContext *ctx)
{
    string text = ctx->getText();
    if (ctx->expression()->getText() == "None")
    {
        // handled in visitFunctionDefinition, since we will not hit this method on implicit 'return None'
    }
    else
    {
        any anyVal = visit(ctx->expression());
        llvm::Value *castVal = anyToValue(anyVal);
        builder->CreateRet(castVal);
    }
    return any();
}

any BalanceVisitor::visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx)
{
    // https://stackoverflow.com/questions/54905211/how-to-implement-function-pointer-by-using-llvm-c-api
    string text = ctx->getText();
    vector<Type *> functionParameterTypes;
    vector<string> functionParameterNames;
    for (BalanceParser::ParameterContext *parameter : ctx->lambda()->parameterList()->parameter())
    {
        string parameterName = parameter->identifier->getText();
        functionParameterNames.push_back(parameterName);
        string typeString = parameter->type->getText();
        Type *type = getBuiltinType(typeString); // TODO: Handle unknown type
        functionParameterTypes.push_back(type);
    }

    // If we don't have a return type, assume none
    Type *returnType;
    if (ctx->lambda()->returnType())
    {
        string functionReturnTypeString = ctx->lambda()->returnType()->IDENTIFIER()->getText();
        returnType = getBuiltinType(functionReturnTypeString); // TODO: Handle unknown type
    }
    else
    {
        returnType = getBuiltinType("None");
    }

    ArrayRef<Type *> parametersReference(functionParameterTypes);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
    Function *function = Function::Create(functionType, Function::InternalLinkage, "", currentModule->module);

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    for (string parameterName : functionParameterNames)
    {
        llvm::Value *x = args++;
        x->setName(parameterName);
        setValue(parameterName, x);
    }

    BasicBlock *functionBody = BasicBlock::Create(*context, "", function);
    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = builder->GetInsertBlock();
    builder->SetInsertPoint(functionBody);
    ScopeBlock *scope = currentModule->currentScope;
    currentModule->currentScope = new ScopeBlock(functionBody, scope);

    visit(ctx->lambda()->functionBlock());

    if (returnType->isVoidTy())
    {
        builder->CreateRetVoid();
    }

    bool hasError = verifyFunction(*function);

    builder->SetInsertPoint(resumeBlock);
    currentModule->currentScope = scope;

    // Create alloca so we can return it as expression
    llvm::Value *p = builder->CreateAlloca(function->getType(), nullptr, "");
    builder->CreateStore(function, p, false);
    return (llvm::Value *)builder->CreateLoad(p);
}

any BalanceVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx)
{
    string functionName = ctx->IDENTIFIER()->getText();
    vector<Type *> functionParameterTypes;
    vector<string> functionParameterNames;
    for (BalanceParser::ParameterContext *parameter : ctx->parameterList()->parameter())
    {
        string parameterName = parameter->identifier->getText();
        functionParameterNames.push_back(parameterName);
        string typeString = parameter->type->getText();
        Type *type = getBuiltinType(typeString); // TODO: Handle unknown type
        functionParameterTypes.push_back(type);
    }

    // If we don't have a return type, assume none
    Type *returnType;
    if (ctx->returnType())
    {
        string functionReturnTypeString = ctx->returnType()->IDENTIFIER()->getText();
        returnType = getBuiltinType(functionReturnTypeString); // TODO: Handle unknown type
    }
    else
    {
        returnType = getBuiltinType("None");
    }

    // Check if we are parsing a class method
    Function *function;
    if (currentClass != nullptr)
    {
        PointerType *thisPointer = currentClass->structType->getPointerTo();
        functionParameterTypes.insert(functionParameterTypes.begin(), thisPointer);
        functionParameterNames.insert(functionParameterNames.begin(), "this");
        string functionNameWithClass = currentClass->name + "_" + functionName;
        ArrayRef<Type *> parametersReference(functionParameterTypes);
        FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
        function = Function::Create(functionType, Function::InternalLinkage, functionNameWithClass, currentModule->module);
        currentClass->methods[functionName] = function;
    }
    else
    {
        ArrayRef<Type *> parametersReference(functionParameterTypes);
        FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
        function = Function::Create(functionType, Function::InternalLinkage, functionName, currentModule->module);
    }

    ScopeBlock *scope = currentModule->currentScope;
    BasicBlock *functionBody = BasicBlock::Create(*context, functionName + "_body", function);
    currentModule->currentScope = new ScopeBlock(functionBody, scope);

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    for (string parameterName : functionParameterNames)
    {
        llvm::Value *x = args++;
        x->setName(parameterName);
        setValue(parameterName, x);
    }

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = builder->GetInsertBlock();
    builder->SetInsertPoint(functionBody);

    visit(ctx->functionBlock());

    if (returnType->isVoidTy())
    {
        builder->CreateRetVoid();
    }

    bool hasError = verifyFunction(*function);

    builder->SetInsertPoint(resumeBlock);
    currentModule->currentScope = scope;
    return nullptr;
}

any BalanceVisitor::visitImportStatement(BalanceParser::ImportStatementContext *ctx) {
    std::string text = ctx->getText();

    std::string importPath;
    if (ctx->IDENTIFIER()) {
        importPath = ctx->IDENTIFIER()->getText();
    } else if (ctx->IMPORT_PATH()) {
        importPath = ctx->IMPORT_PATH()->getText();
    } else {
        // TODO: Handle this with an error
    }

    BalanceModule * importModule = currentPackage->modules[importPath];

    for (BalanceParser::ImportDefinitionContext *parameter : ctx->importDefinitionList()->importDefinition())
    {
        if (dynamic_cast<BalanceParser::UnnamedImportDefinitionContext *>(parameter)) {
            BalanceParser::UnnamedImportDefinitionContext * import = dynamic_cast<BalanceParser::UnnamedImportDefinitionContext *>(parameter);
            std::string importString = import->IDENTIFIER()->getText();
            Value * val = importModule->getValue(importString);
            setValue(importString, val);
        }
    }

    // 

    // map<string, BalanceModule *>::iterator it = currentPackage->modules.find(importPath);
    // if (currentPackage->modules.end() == it) {
    //      = new BalanceModule(importPath, false);
    // }

    return nullptr;
}