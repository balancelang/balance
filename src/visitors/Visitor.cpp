#include "Visitor.h"
#include "../Package.h"
#include "../models/BalanceClass.h"

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
extern llvm::Value *accessedValue;
extern std::map<llvm::Value *, BalanceTypeString *> typeLookup;

void LogError(std::string errorMessage) { fprintf(stderr, "Error: %s\n", errorMessage.c_str()); }

llvm::Value *anyToValue(any anyVal) { return any_cast<llvm::Value *>(anyVal); }

Type *getBuiltinType(BalanceTypeString * typeString) {
    if (typeString->base == "Int") {
        return Type::getInt32Ty(*currentPackage->context);
    } else if (typeString->base == "Bool") {
        return Type::getInt1Ty(*currentPackage->context);
    } else if (typeString->base == "String") {
        return currentPackage->builtins->getClass("String")->structType->getPointerTo();
    } else if (typeString->base == "Double") {
        return Type::getDoubleTy(*currentPackage->context);
    } else if (typeString->base == "None") {
        return Type::getVoidTy(*currentPackage->context);
    } else if (typeString->base == "Array") {
        return currentPackage->builtins->getClass("Array")->structType->getPointerTo();
    }

    return nullptr;
}

Constant *geti8StrVal(Module &M, char const *str, Twine const &name) {
    Constant *strConstant = ConstantDataArray::getString(M.getContext(), str);
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
    std::string className = ctx->classInitializer()->IDENTIFIER()->getText();

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
        // TODO: throw error
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
    return (Value *)structMemoryPointer;
}

any BalanceVisitor::visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) {
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
    any expressionResult = visit(ctx->expression());
    llvm::Value *expression = anyToValue(expressionResult);

    // Create the condition - if expression is true, jump to loop block, else jump to after loop block
    currentPackage->currentModule->builder->CreateCondBr(expression, loopBlock, mergeBlock);

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
    return any();
}

any BalanceVisitor::visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) {
    std::string text = ctx->getText();

    any anyValMember = visit(ctx->member);
    llvm::Value *valueMember = anyToValue(anyValMember);
    any anyValIndex = visit(ctx->index);
    llvm::Value *valueIndex = anyToValue(anyValIndex); // Should return i32 for now
    any anyVal = visit(ctx->value);
    llvm::Value *value = anyToValue(anyVal);

    auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember, {zero, valueIndex});
    currentPackage->currentModule->builder->CreateStore(value, ptr);

    return any();
}

any BalanceVisitor::visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) {
    std::string text = ctx->getText();
    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();

    any anyValMember = visit(ctx->member);
    llvm::Value *valueMember = anyToValue(anyValMember);
    any anyValIndex = visit(ctx->index);
    llvm::Value *valueIndex = anyToValue(anyValIndex); // Should return i32 for now

    auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto ptr = currentPackage->currentModule->builder->CreateGEP(valueMember, {zero, valueIndex});
    return (Value *)currentPackage->currentModule->builder->CreateLoad(ptr);
}

any BalanceVisitor::visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) {
    std::string text = ctx->getText();
    vector<llvm::Value *> values;
    for (BalanceParser::ExpressionContext *expression : ctx->listElements()->expression()) {
        any expressionResult = visit(expression);
        llvm::Value *value = anyToValue(expressionResult);
        values.push_back(value);
    }

    Type *type = values[0]->getType();
    auto elementSize = ConstantExpr::getSizeOf(type);
    BalanceClass *arrayClass = currentPackage->builtins->getClass("Array");
    auto arrayLength = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*currentPackage->context), values.size());
    auto allocize = llvm::ConstantExpr::getMul(elementSize, arrayLength);

    auto memoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),           // input type?
        type,                                                       // output type, which we get pointer to?
        allocize,                                                   // size, matches input type?
        nullptr,
        nullptr,
        ""
    );
    currentPackage->currentModule->builder->Insert(memoryPointer);

    for (int i = 0; i < values.size(); i++) {
        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, i, true));
        auto ptr = currentPackage->currentModule->builder->CreateGEP(memoryPointer, {index});
        currentPackage->currentModule->builder->CreateStore(values[i], ptr);
    }

    auto arrayMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),       // input type?
        arrayClass->structType,                                // output type, which we get pointer to?
        ConstantExpr::getSizeOf(arrayClass->structType),       // size, matches input type?
        nullptr, nullptr, "");
    currentPackage->currentModule->builder->Insert(arrayMemoryPointer);
    ArrayRef<Value *> constructorArguments{arrayMemoryPointer};
    currentPackage->currentModule->builder->CreateCall(arrayClass->constructor, constructorArguments);

    // length
    auto lengthZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto lengthIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayClass->properties["length"]->index, true));
    auto lengthGEP = currentPackage->currentModule->builder->CreateGEP(arrayClass->structType, arrayMemoryPointer, {lengthZeroValue, lengthIndexValue});
    currentPackage->currentModule->builder->CreateStore(arrayLength, lengthGEP);

    // elementsize
    auto elementSizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto elementSizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayClass->properties["elementSize"]->index, true));
    auto elementSizeGEP = currentPackage->currentModule->builder->CreateGEP(arrayClass->structType, arrayMemoryPointer, {elementSizeZeroValue, elementSizeIndexValue});
    currentPackage->currentModule->builder->CreateStore(elementSize, elementSizeGEP);

    // memorypointer
    auto memoryPointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto memoryPointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, arrayClass->properties["memoryPointer"]->index, true));
    auto memoryPointerGEP = currentPackage->currentModule->builder->CreateGEP(arrayClass->structType, arrayMemoryPointer, {memoryPointerZeroValue, memoryPointerIndexValue});
    currentPackage->currentModule->builder->CreateStore(memoryPointer, memoryPointerGEP);

    return (Value *)arrayMemoryPointer;
}

any BalanceVisitor::visitIfStatement(BalanceParser::IfStatementContext *ctx) {
    std::string text = ctx->getText();
    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();

    any expressionResult = visit(ctx->expression());
    llvm::Value *expression = anyToValue(expressionResult);
    BasicBlock *thenBlock = BasicBlock::Create(*currentPackage->context, "then", function);
    BasicBlock *elseBlock = BasicBlock::Create(*currentPackage->context, "else");
    BasicBlock *mergeBlock = BasicBlock::Create(*currentPackage->context, "ifcont");
    currentPackage->currentModule->builder->CreateCondBr(expression, thenBlock, elseBlock);

    currentPackage->currentModule->builder->SetInsertPoint(thenBlock);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(thenBlock, scope);
    visit(ctx->ifblock);
    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    thenBlock = currentPackage->currentModule->builder->GetInsertBlock();
    function->getBasicBlockList().push_back(elseBlock);
    currentPackage->currentModule->builder->SetInsertPoint(elseBlock);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(elseBlock, scope);
    if (ctx->elseblock) {
        visit(ctx->elseblock);
    }
    currentPackage->currentModule->builder->CreateBr(mergeBlock);
    elseBlock = currentPackage->currentModule->builder->GetInsertBlock();
    function->getBasicBlockList().push_back(mergeBlock);
    currentPackage->currentModule->builder->SetInsertPoint(mergeBlock);
    currentPackage->currentModule->currentScope = scope;
    return any();
}

any BalanceVisitor::visitVariableExpression(BalanceParser::VariableExpressionContext *ctx) {
    std::string variableName = ctx->variable()->IDENTIFIER()->getText();

    if (accessedValue != nullptr) {
        if (PointerType *PT = dyn_cast<PointerType>(accessedValue->getType())) {
            if (PT->getPointerElementType()->isStructTy()) {
                // get struct type
                std::string className = PT->getPointerElementType()->getStructName().str();
                // TODO: Make a function that does this search
                BalanceClass *bclass = currentPackage->currentModule->getClass(className);
                if (bclass == nullptr) {
                    BalanceImportedClass *ibClass = currentPackage->currentModule->getImportedClass(className);
                    if (ibClass == nullptr) {
                        // TODO: Throw error.
                    } else {
                        bclass = ibClass->bclass;
                    }
                }

                // check if it is a public property
                BalanceProperty *bproperty = bclass->getProperty(variableName);
                if (bproperty == nullptr) {
                    // TODO: Handle property not existing
                }

                if (!bproperty->isPublic) {
                    // TODO: Handle property not being public (e.g. accessible from Balance)
                }

                Value *zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
                Value *propertyIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, bproperty->index, true));
                Value *propertyPointerValue = currentPackage->currentModule->builder->CreateGEP(bclass->structType, accessedValue, {zero, propertyIndex});
                return (Value *)currentPackage->currentModule->builder->CreateLoad(propertyPointerValue);
            }
        }
    }

    Value *val = currentPackage->currentModule->getValue(variableName);
    if (val == nullptr && currentPackage->currentModule->currentClass != nullptr) {
        // TODO: Check if variableName is in currentClass->properties
        int intIndex = currentPackage->currentModule->currentClass->properties[variableName]->index;
        auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));
        Value *thisValue = currentPackage->currentModule->getValue("this");
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue, {zero, index});
        return (Value *)currentPackage->currentModule->builder->CreateLoad(ptr);
    }

    if (val->getType()->isPointerTy() && !val->getType()->getPointerElementType()->isStructTy()) {
        return (Value *)currentPackage->currentModule->builder->CreateLoad(val);
    }
    return val;
}

any BalanceVisitor::visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) {
    std::string text = ctx->getText();
    std::string variableName = ctx->IDENTIFIER()->getText();
    any expressionResult = visit(ctx->expression());
    Value *value = anyToValue(expressionResult);

    // We don't malloc here, since we're just allocating a pointer to the struct (malloced in visitClassInitializerExpression)
    Value *alloca = currentPackage->currentModule->builder->CreateAlloca(value->getType());
    currentPackage->currentModule->builder->CreateStore(value, alloca);
    currentPackage->currentModule->setValue(variableName, alloca);
    return alloca;
}

any BalanceVisitor::visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) {
    // TODO: Reference counting could free up memory here

    std::string text = ctx->getText();
    Function *function = currentPackage->currentModule->builder->GetInsertBlock()->getParent();
    std::string variableName = ctx->IDENTIFIER()->getText();
    any expressionResult = visit(ctx->expression());
    llvm::Value *value = anyToValue(expressionResult);

    Value *variable = currentPackage->currentModule->getValue(variableName);
    if (variable == nullptr && currentPackage->currentModule->currentClass != nullptr) {
        // TODO: Check if variableName is in currentClass->properties
        int intIndex = currentPackage->currentModule->currentClass->properties[variableName]->index;
        auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));
        Value *thisValue = currentPackage->currentModule->getValue("this");
        Type *structType = thisValue->getType()->getPointerElementType();

        auto ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisValue, {zero, index});
        return (Value *)currentPackage->currentModule->builder->CreateStore(value, ptr);
    }

    return (Value *)currentPackage->currentModule->builder->CreateStore(value, variable);
}

any BalanceVisitor::visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx) {
    any lhs = visit(ctx->lhs);
    any rhs = visit(ctx->rhs);

    llvm::Value *lhsVal = anyToValue(lhs);
    llvm::Value *rhsVal = anyToValue(rhs);

    if (ctx->LT()) {
        return (Value *)currentPackage->currentModule->builder->CreateICmpSLT(lhsVal, rhsVal, "lttmp");
    } else if (ctx->GT()) {
        return (Value *)currentPackage->currentModule->builder->CreateICmpSGT(lhsVal, rhsVal, "gttmp");
    } else if (ctx->OP_LE()) {
        return (Value *)currentPackage->currentModule->builder->CreateICmpSLE(lhsVal, rhsVal, "letmp");
    } else if (ctx->OP_GE()) {
        return (Value *)currentPackage->currentModule->builder->CreateICmpSGE(lhsVal, rhsVal, "getmp");
    }

    return any();
}

any BalanceVisitor::visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) {
    any lhs = visit(ctx->lhs);
    any rhs = visit(ctx->rhs);

    llvm::Value *lhsVal = anyToValue(lhs);
    llvm::Value *rhsVal = anyToValue(rhs);

    // If either operand is float, cast other to float (consolidate this whole thing)
    if (lhsVal->getType()->isFloatingPointTy() || rhsVal->getType()->isFloatingPointTy()) {
        if (!lhsVal->getType()->isFloatingPointTy()) {
            if (lhsVal->getType()->isIntegerTy()) {
                int width = lhsVal->getType()->getIntegerBitWidth();
                if (width == 1) {
                    // bool
                    lhsVal = currentPackage->currentModule->builder->CreateUIToFP(lhsVal, getBuiltinType(new BalanceTypeString("Double")));
                } else if (width == 32) {
                    // int32
                    lhsVal = currentPackage->currentModule->builder->CreateSIToFP(lhsVal, getBuiltinType(new BalanceTypeString("Double")));
                }
            }
        }
        if (!rhsVal->getType()->isFloatingPointTy()) {
            if (rhsVal->getType()->isIntegerTy()) {
                int width = rhsVal->getType()->getIntegerBitWidth();
                if (width == 1) {
                    // bool
                    rhsVal = currentPackage->currentModule->builder->CreateUIToFP(rhsVal, getBuiltinType(new BalanceTypeString("Double")));
                } else if (width == 32) {
                    // int32
                    rhsVal = currentPackage->currentModule->builder->CreateSIToFP(rhsVal, getBuiltinType(new BalanceTypeString("Double")));
                }
            }
        }
    }

    if (ctx->STAR()) {
        // Just check one of them, since both will be floats or none.
        if (lhsVal->getType()->isFloatingPointTy()) {
            return (Value *)currentPackage->currentModule->builder->CreateFMul(lhsVal, rhsVal);
        }
        return (Value *)currentPackage->currentModule->builder->CreateMul(lhsVal, rhsVal);
    } else {
        if (lhsVal->getType()->isFloatingPointTy()) {
            return (Value *)currentPackage->currentModule->builder->CreateFDiv(lhsVal, rhsVal);
        }
        return (Value *)currentPackage->currentModule->builder->CreateSDiv(lhsVal, rhsVal);
    }

    return any();
}

any BalanceVisitor::visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx) {
    any lhs = visit(ctx->lhs);
    any rhs = visit(ctx->rhs);

    llvm::Value *lhsVal = anyToValue(lhs);
    llvm::Value *rhsVal = anyToValue(rhs);

    // If either operand is float, cast other to float (consolidate this whole thing)
    if (lhsVal->getType()->isFloatingPointTy() || rhsVal->getType()->isFloatingPointTy()) {
        if (!lhsVal->getType()->isFloatingPointTy()) {
            if (lhsVal->getType()->isIntegerTy()) {
                int width = lhsVal->getType()->getIntegerBitWidth();
                if (width == 1) {
                    // bool
                    lhsVal = currentPackage->currentModule->builder->CreateUIToFP(lhsVal, getBuiltinType(new BalanceTypeString("Double")));
                } else if (width == 32) {
                    // int32
                    lhsVal = currentPackage->currentModule->builder->CreateSIToFP(lhsVal, getBuiltinType(new BalanceTypeString("Double")));
                }
            }
        }
        if (!rhsVal->getType()->isFloatingPointTy()) {
            if (rhsVal->getType()->isIntegerTy()) {
                int width = rhsVal->getType()->getIntegerBitWidth();
                if (width == 1) {
                    // bool
                    rhsVal = currentPackage->currentModule->builder->CreateUIToFP(rhsVal, getBuiltinType(new BalanceTypeString("Double")));
                } else if (width == 32) {
                    // int32
                    rhsVal = currentPackage->currentModule->builder->CreateSIToFP(rhsVal, getBuiltinType(new BalanceTypeString("Double")));
                }
            }
        }
    }

    if (ctx->PLUS()) {
        // Just check one of them, since both will be floats or none.
        if (lhsVal->getType()->isFloatingPointTy()) {
            return (Value *)currentPackage->currentModule->builder->CreateFAdd(lhsVal, rhsVal);
        }
        return (Value *)currentPackage->currentModule->builder->CreateAdd(lhsVal, rhsVal);
    } else {
        if (lhsVal->getType()->isFloatingPointTy()) {
            return (Value *)currentPackage->currentModule->builder->CreateFSub(lhsVal, rhsVal);
        }
        return (Value *)currentPackage->currentModule->builder->CreateSub(lhsVal, rhsVal);
    }

    return any();
}

any BalanceVisitor::visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) {
    std::string value = ctx->DECIMAL_INTEGER()->getText();
    Type *i32_type = IntegerType::getInt32Ty(*currentPackage->context);
    int intValue = stoi(value);
    return (Value *)ConstantInt::get(i32_type, intValue, true);
}

any BalanceVisitor::visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) {
    Type *type = getBuiltinType(new BalanceTypeString("Bool"));
    if (ctx->TRUE()) {
        return (Value *)ConstantInt::get(type, 1, true);
    }
    return (Value *)ConstantInt::get(type, 0, true);
}

any BalanceVisitor::visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) {
    std::string value = ctx->DOUBLE()->getText();
    Type *type = getBuiltinType(new BalanceTypeString("Double"));
    double doubleValue = stod(value);
    return (Value *)ConstantFP::get(type, doubleValue);
}

any BalanceVisitor::visitStringLiteral(BalanceParser::StringLiteralContext *ctx) {
    std::string text = ctx->STRING()->getText();
    int stringLength = text.size();

    BalanceImportedClass *ibclass = currentPackage->currentModule->getImportedClass("String");

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

    return (Value *)stringMemoryPointer;
}

any BalanceVisitor::visitFunctionCall(BalanceParser::FunctionCallContext *ctx) {
    std::string text = ctx->getText();
    std::string functionName = ctx->IDENTIFIER()->getText();
    // TODO: Make a search through scope, parent scopes, builtins etc.
    if (accessedValue == nullptr && functionName == "print") {
        // FunctionCallee printfFunc = currentPackage->builtins->module->getFunction("printf");
        FunctionCallee printFunc = currentPackage->currentModule->getImportedFunction("print")->function;
        vector<Value *> functionArguments;
        for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
            any anyVal = visit(argument);
            llvm::Value *value = anyToValue(anyVal);
            if (PointerType *PT = dyn_cast<PointerType>(value->getType())) {
                if (PT->getPointerElementType()->isStructTy()) {
                    std::string structName = PT->getPointerElementType()->getStructName().str();
                    if (structName == "String") {
                        auto args = ArrayRef<Value *>{value};
                        return (Value *)currentPackage->currentModule->builder->CreateCall(printFunc, args);
                    } else {
                        // invoke .toString() on the struct
                        BalanceClass * bclass = currentPackage->currentModule->getClass(structName);
                        if (bclass == nullptr) {
                            BalanceImportedClass * ibclass = currentPackage->currentModule->getImportedClass(structName);
                            if (ibclass == nullptr) {
                                // TODO: Throw error
                            } else {
                                bclass = ibclass->bclass;
                            }
                        }

                        Function * toStringFunction = bclass->methods["toString"]->function;
                        auto args = ArrayRef<Value *>{value};
                        Value *stringValue = currentPackage->currentModule->builder->CreateCall(toStringFunction, args);

                        auto printArgs = ArrayRef<Value *>{stringValue};
                        return (Value *)currentPackage->currentModule->builder->CreateCall(printFunc, printArgs);
                    }
                } else if (PT->getElementType()->isArrayTy()) {
                    // TODO: Implement Array.toString() and invoke that instead of directly calling printf
                    auto zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
                    auto argsBefore = ArrayRef<llvm::Value *>{geti8StrVal(*currentPackage->currentModule->module, "[", "args")};
                    FunctionCallee printfFunc = currentPackage->builtins->module->getFunction("printf");
                    (llvm::Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, argsBefore);
                    int numElements = PT->getElementType()->getArrayNumElements();
                    // TODO: Optimize this, so we generate ONE string e.g. "%d, %d, %d, %d\n"
                    // Also, make a function that does this, which takes value, type and whether to linebreak?
                    for (int i = 0; i < numElements; i++) {
                        auto index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, i, true));
                        auto ptr = currentPackage->currentModule->builder->CreateGEP(value, {zero, index});
                        llvm::Value *valueAtIndex = (Value *)currentPackage->currentModule->builder->CreateLoad(ptr);
                        if (i < numElements - 1) {
                            auto args = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "%d, ", "args"), valueAtIndex};
                            (Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, args);
                        } else {
                            auto args = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "%d", "args"), valueAtIndex};
                            (Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, args);
                        }
                    }
                    auto argsAfter = ArrayRef<Value *>{geti8StrVal(*currentPackage->currentModule->module, "]\n", "args")};
                    (Value *)currentPackage->currentModule->builder->CreateCall(printfFunc, argsAfter);

                    return any();
                }
            } else if (IntegerType *IT = dyn_cast<IntegerType>(value->getType())) {
                int width = value->getType()->getIntegerBitWidth();
                if (width == 1) {
                    BalanceImportedClass *boolClass = currentPackage->currentModule->getImportedClass("Bool");
                    BalanceImportedFunction *boolToStringFunc = boolClass->methods["toString"];
                    auto args = ArrayRef<Value *>{value};
                    Value *stringValue = currentPackage->currentModule->builder->CreateCall(boolToStringFunc->function, args);

                    auto printArgs = ArrayRef<Value *>{stringValue};
                    return (Value *)currentPackage->currentModule->builder->CreateCall(printFunc, printArgs);
                } else {
                    BalanceImportedClass *intClass = currentPackage->currentModule->getImportedClass("Int");
                    BalanceImportedFunction *intToStringFunc = intClass->methods["toString"];
                    auto args = ArrayRef<Value *>{value};
                    Value *stringValue = currentPackage->currentModule->builder->CreateCall(intToStringFunc->function, args);

                    auto printArgs = ArrayRef<Value *>{stringValue};
                    return (Value *)currentPackage->currentModule->builder->CreateCall(printFunc, printArgs);
                }
            } else if (value->getType()->isFloatingPointTy()) {
                BalanceImportedClass *doubleClass = currentPackage->currentModule->getImportedClass("Double");
                BalanceImportedFunction *doubleToStringFunc = doubleClass->methods["toString"];
                auto args = ArrayRef<Value *>{value};
                Value *stringValue = currentPackage->currentModule->builder->CreateCall(doubleToStringFunc->function, args);

                auto printArgs = ArrayRef<Value *>{stringValue};
                return (Value *)currentPackage->currentModule->builder->CreateCall(printFunc, printArgs);
            }
            break;
        }
    } else if (accessedValue == nullptr && functionName == "open") {
        Function *fopenFunc = currentPackage->builtins->module->getFunction("fopen"); // TODO: Move this to separate module
        BalanceClass *stringClass = currentPackage->builtins->getClass("String");
        vector<Value *> functionArguments;
        for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
            any anyVal = visit(argument);
            llvm::Value *value = anyToValue(anyVal);
            functionArguments.push_back(value);
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

        return (Value *)pointer;
    } else {
        if (accessedValue != nullptr) {
            // Means we're invoking function on an element
            Type *elementType = accessedValue->getType();
            vector<Value *> functionArguments;
            std::string className;

            if (PointerType *PT = dyn_cast<PointerType>(accessedValue->getType())) {
                // TODO: Check that it is actually struct
                className = PT->getElementType()->getStructName().str();
            } else if (elementType->isIntegerTy()) {
                int width = elementType->getIntegerBitWidth();
                if (width == 1) {
                    className = "Bool";
                } else if (width == 32) {
                    className = "Int";
                }
            } else if (elementType->isFloatingPointTy()) {
                className = "Double";
            }

            // Add "this" as first argument
            functionArguments.push_back(accessedValue);

            // Don't consider accessedValue when parsing arguments
            llvm::Value * backup = accessedValue;
            accessedValue = nullptr;
            for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
                any anyVal = visit(argument);
                llvm::Value *castVal = anyToValue(anyVal);
                functionArguments.push_back(castVal);
            }
            accessedValue = backup;

            Function *function;
            // TODO: Make a function that does this search and else returns nullptr
            BalanceClass *bclass = currentPackage->currentModule->getClass(className);
            if (bclass == nullptr) {
                BalanceImportedClass *ibClass = currentPackage->currentModule->getImportedClass(className);
                if (ibClass == nullptr) {
                    bclass = currentPackage->builtins->getClass(className);
                    if (bclass == nullptr) {
                        // TODO: Throw error.
                    } else {
                        function = bclass->methods[functionName]->function;
                    }
                } else {
                    function = ibClass->methods[functionName]->function;
                }
            } else {
                function = bclass->methods[functionName]->function;
            }
            FunctionType *functionType = function->getFunctionType();

            ArrayRef<Value *> argumentsReference(functionArguments);
            return (Value *)currentPackage->currentModule->builder->CreateCall(functionType, (Value *)function, argumentsReference);
        } else {
            // TODO: Handle
        }
    }

    // Check if function is a variable (lambda e.g.)
    llvm::Value *val = currentPackage->currentModule->getValue(functionName);
    if (val) {
        auto ty = val->getType();
        if (val->getType()->getPointerElementType()->isFunctionTy()) {
            vector<Value *> functionArguments;

            for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
                any anyVal = visit(argument);
                llvm::Value *castVal = anyToValue(anyVal);
                functionArguments.push_back(castVal);
            }

            ArrayRef<Value *> argumentsReference(functionArguments);
            return (Value *)currentPackage->currentModule->builder->CreateCall(FunctionType::get(val->getType(), false), val, argumentsReference);
        } else if (val->getType()->getPointerElementType()->isPointerTy()) {
            Value *loaded = currentPackage->currentModule->builder->CreateLoad(val);
            vector<Value *> functionArguments;

            for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
                any anyVal = visit(argument);
                llvm::Value *castVal = anyToValue(anyVal);
                functionArguments.push_back(castVal);
            }

            ArrayRef<Value *> argumentsReference(functionArguments);
            return (Value *)currentPackage->currentModule->builder->CreateCall(FunctionType::get(loaded->getType(), false), loaded, argumentsReference);
        }
    } else {
        FunctionCallee function;

        BalanceFunction *bfunction = currentPackage->currentModule->getFunction(functionName);
        if (bfunction == nullptr) {
            BalanceImportedFunction *ibfunction = currentPackage->currentModule->getImportedFunction(functionName);
            if (ibfunction == nullptr) {
                // TODO: Throw error
            } else {
                function = ibfunction->function;
            }
        } else {
            function = bfunction->function;
        }

        vector<Value *> functionArguments;

        for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
            any anyVal = visit(argument);
            llvm::Value *castVal = anyToValue(anyVal);
            functionArguments.push_back(castVal);
        }

        ArrayRef<Value *> argumentsReference(functionArguments);
        return (Value *)currentPackage->currentModule->builder->CreateCall(function, argumentsReference);
    }

    return any();
}

any BalanceVisitor::visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) {
    std::string text = ctx->getText();
    if (ctx->expression()->getText() == "None") {
        // handled in visitFunctionDefinition, since we will not hit this method on implicit 'return None'
    } else {
        any anyVal = visit(ctx->expression());
        llvm::Value *castVal = anyToValue(anyVal);
        currentPackage->currentModule->builder->CreateRet(castVal);
    }
    return any();
}

any BalanceVisitor::visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx) {
    // https://stackoverflow.com/questions/54905211/how-to-implement-function-pointer-by-using-llvm-c-api
    std::string text = ctx->getText();
    vector<Type *> functionParameterTypes;
    vector<std::string> functionParameterNames;
    for (BalanceParser::ParameterContext *parameter : ctx->lambda()->parameterList()->parameter()) {
        std::string parameterName = parameter->identifier->getText();
        functionParameterNames.push_back(parameterName);
        std::string typeString = parameter->type->getText();
        Type *type = getBuiltinType(new BalanceTypeString(typeString)); // TODO: Handle unknown type
        functionParameterTypes.push_back(type);
    }

    // If we don't have a return type, assume none
    Type *returnType;
    if (ctx->lambda()->returnType()) {
        std::string functionReturnTypeString = ctx->lambda()->returnType()->balanceType()->getText();
        returnType = getBuiltinType(new BalanceTypeString(functionReturnTypeString)); // TODO: Handle unknown type
    } else {
        returnType = getBuiltinType(new BalanceTypeString("None"));
    }

    ArrayRef<Type *> parametersReference(functionParameterTypes);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
    Function *function = Function::Create(functionType, Function::ExternalLinkage, "", currentPackage->currentModule->module);

    // Add parameter names
    Function::arg_iterator args = function->arg_begin();
    for (std::string parameterName : functionParameterNames) {
        llvm::Value *x = args++;
        x->setName(parameterName);
        currentPackage->currentModule->setValue(parameterName, x);
    }

    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, "", function);
    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);
    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(functionBody, scope);

    visit(ctx->lambda()->functionBlock());

    if (returnType->isVoidTy()) {
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
    return (llvm::Value *)currentPackage->currentModule->builder->CreateLoad(p);
}

any BalanceVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) {
    std::string functionName = ctx->IDENTIFIER()->getText();
    BalanceFunction *bfunction;

    if (currentPackage->currentModule->currentClass != nullptr) {
        bfunction = currentPackage->currentModule->currentClass->methods[functionName];
    } else {
        bfunction = currentPackage->currentModule->getFunction(functionName);
    }

    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", bfunction->function);
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(functionBody, scope);

    // Add function parameter names and insert in function scope
    Function::arg_iterator args = bfunction->function->arg_begin();
    if (currentPackage->currentModule->currentClass != nullptr) {
        llvm::Value *thisPointer = args++;
        currentPackage->currentModule->setValue("this", thisPointer);
    }

    for (BalanceParameter *parameter : bfunction->parameters) {
        llvm::Value *x = args++;
        x->setName(parameter->name);
        currentPackage->currentModule->setValue(parameter->name, x);
    }

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    visit(ctx->functionBlock());

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