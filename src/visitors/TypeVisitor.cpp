#include "TypeVisitor.h"
#include "../Package.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::string TypeVisitor::getText(antlr4::ParserRuleContext *ctx) {
    int a = ctx->start->getStartIndex();
    int b = ctx->stop->getStopIndex();
    antlr4::misc::Interval * interval = new antlr4::misc::Interval((size_t) a, (size_t) b);
    return currentPackage->currentModule->antlrStream->getText(*interval);
}

std::any TypeVisitor::visitWhileStatement(BalanceParser::WhileStatementContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;

    BalanceTypeString * condition = any_cast<BalanceTypeString *>(visit(ctx->expression()));
    if (condition->base != "Bool") {
        currentPackage->currentModule->addTypeError(ctx, "While condition must be bool: " + this->getText(ctx->expression()));
    }

    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, currentPackage->currentModule->currentScope);

    // Visit the while-block statements
    visit(ctx->ifBlock());

    currentPackage->currentModule->currentScope = scope;

    return std::any();
}

std::any TypeVisitor::visitIfStatement(BalanceParser::IfStatementContext *ctx) {
    std::string text = this->getText(ctx);
    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;

    BalanceTypeString * condition = any_cast<BalanceTypeString *>(visit(ctx->expression()));
    if (condition->base != "Bool") {
        currentPackage->currentModule->addTypeError(ctx, "If statement condition must be bool: " + this->getText(ctx->expression()));
    }

    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, scope);
    visit(ctx->ifblock);

    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, scope);
    if (ctx->elseblock) {
        visit(ctx->elseblock);
    }

    // Reset scope
    currentPackage->currentModule->currentScope = scope;
    return std::any();
}

std::any TypeVisitor::visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) {
    std::string text = this->getText(ctx);

    if (currentPackage->currentModule->currentFunction == nullptr && currentPackage->currentModule->currentLambda == nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Can't return here.");
    }

    BalanceTypeString * returnValue = any_cast<BalanceTypeString *>(visit(ctx->expression()));

    if (currentPackage->currentModule->currentFunction != nullptr) {
        if (!returnValue->equalTo(currentPackage->currentModule->currentFunction->returnTypeString)) {
            currentPackage->currentModule->addTypeError(ctx, "Return types differ. Expected " + currentPackage->currentModule->currentFunction->returnTypeString->toString() + ", found " + returnValue->toString());
        }
        currentPackage->currentModule->currentFunction->hasExplicitReturn = true;
    }
    if (currentPackage->currentModule->currentLambda != nullptr) {
        if (!returnValue->equalTo(currentPackage->currentModule->currentLambda->returnTypeString)) {
            currentPackage->currentModule->addTypeError(ctx, "Return types differ. Expected " + currentPackage->currentModule->currentLambda->returnTypeString->toString() + ", found " + returnValue->toString());
        }
        currentPackage->currentModule->currentLambda->hasExplicitReturn = true;
    }

    return std::any();
}

std::any TypeVisitor::visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) {
    std::string text = this->getText(ctx);
    std::string variableName = ctx->IDENTIFIER()->getText();

    BalanceTypeString *tryVal = currentPackage->currentModule->currentScope->typeSymbolTable[variableName];
    if (tryVal != nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Duplicate variable name: " + variableName);
        return std::any();
    }

    BalanceTypeString * value = any_cast<BalanceTypeString *>(visit(ctx->expression()));

    currentPackage->currentModule->currentScope->typeSymbolTable[variableName] = value;

    return std::any();
}

std::any TypeVisitor::visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) {
    std::string text = this->getText(ctx);
    std::string variableName = ctx->IDENTIFIER()->getText();
    BalanceTypeString *tryVal = currentPackage->currentModule->getTypeValue(variableName);
    if (tryVal == nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Unknown variable: " + variableName);
        return new BalanceTypeString("Unknown");
    }

    BalanceTypeString * expression = any_cast<BalanceTypeString *>(visit(ctx->expression()));
    if (!tryVal->equalTo(expression)) {
        currentPackage->currentModule->addTypeError(ctx, "Types are not equal in reassignment: " + text + ", " + tryVal->toString() + " != " + expression->toString());
    }

    return std::any();
}

// std::any TypeVisitor::visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) { return std::any(); }

std::any TypeVisitor::visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) {
    std::string text = ctx->getText();

    BalanceTypeString * accessedType = any_cast<BalanceTypeString *>(visit(ctx->member));

    // Store this value, so that visiting the access will use it as context
    BalanceClass * bclass = currentPackage->currentModule->getClass(accessedType);
    if (bclass == nullptr) {
        BalanceImportedClass * ibclass = currentPackage->currentModule->getImportedClass(accessedType);
        if (ibclass == nullptr) {
            // TODO: Handle
        } else {
            currentPackage->currentModule->accessedType = ibclass->bclass;
        }
    } else {
        currentPackage->currentModule->accessedType = bclass;
    }

    BalanceTypeString *returnType = any_cast<BalanceTypeString *>(visit(ctx->access));
    currentPackage->currentModule->accessedType = nullptr;
    return returnType;
}

std::any TypeVisitor::visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceTypeString * lhs = any_cast<BalanceTypeString *>(visit(ctx->lhs));
    BalanceTypeString * rhs = any_cast<BalanceTypeString *>(visit(ctx->rhs));

    if (!lhs->equalTo(rhs)) {
        currentPackage->currentModule->addTypeError(ctx, "Types are not equal in additive expression: " + text + ", " + lhs->toString() + " != " + rhs->toString());
    }

    return lhs;
}

std::any TypeVisitor::visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceTypeString * lhs = any_cast<BalanceTypeString *>(visit(ctx->lhs));
    BalanceTypeString * rhs = any_cast<BalanceTypeString *>(visit(ctx->rhs));

    if (!lhs->equalTo(rhs)) {
        currentPackage->currentModule->addTypeError(ctx, "Types are not equal in relational expression: " + text + ", " + lhs->toString() + " != " + rhs->toString());
    }

    return new BalanceTypeString("Bool");
}
// std::any TypeVisitor::visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) { return std::any(); }

std::any TypeVisitor::visitVariable(BalanceParser::VariableContext *ctx) {
    std::string variableName = ctx->IDENTIFIER()->getText();
    BalanceTypeString *tryVal = currentPackage->currentModule->getTypeValue(variableName);
    if (tryVal == nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Unknown variable: " + variableName);
        return new BalanceTypeString("Unknown");
    }
    return tryVal;
}
// std::any TypeVisitor::visitParameterList(BalanceParser::ParameterListContext *ctx) { return std::any(); }


std::any TypeVisitor::visitGenericType(BalanceParser::GenericTypeContext *ctx) {
    std::string base = ctx->base->getText();
    std::vector<BalanceTypeString *> generics;

    for (BalanceParser::BalanceTypeContext *type: ctx->typeList()->balanceType()) {
        BalanceTypeString * typeString = any_cast<BalanceTypeString *>(visit(type));
        generics.push_back(typeString);
    }


    // TODO: Figure out how we test if the class supports the generics

    return new BalanceTypeString(base, generics);
}
// std::any TypeVisitor::visitTypeList(BalanceParser::TypeListContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitParameter(BalanceParser::ParameterContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitArgumentList(BalanceParser::ArgumentListContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitArgument(BalanceParser::ArgumentContext *ctx) { return std::any(); }

std::any TypeVisitor::visitFunctionCall(BalanceParser::FunctionCallContext *ctx) {
    std::string text = this->getText(ctx);
    std::string functionName = ctx->IDENTIFIER()->getText();

    BalanceTypeString * returnType = nullptr;
    vector<BalanceTypeString *> expectedParameters;
    vector<BalanceTypeString *> actualParameters;

    // Check if we are accessing a type
    if (currentPackage->currentModule->accessedType != nullptr) {
        // check if function exists
        BalanceFunction * bfunction = currentPackage->currentModule->accessedType->methods[functionName];
        if (bfunction == nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Unknown method. Type " + currentPackage->currentModule->accessedType->name->toString() + " does not have a method called " + functionName);
            return new BalanceTypeString("Unknown");
        }
        returnType = bfunction->returnTypeString;

        // TODO: Remove 'this'
        for (int i = 1; i < bfunction->parameters.size(); i++) {
            expectedParameters.push_back(bfunction->parameters[i]->balanceTypeString);
        }
    } else {
        BalanceTypeString * lambda = currentPackage->currentModule->getTypeValue(functionName);

        // Check if we have a lambda in scope, else it must be a function
        if (lambda != nullptr) {
            returnType = lambda->generics.back();

            for (int i = 0; i < lambda->generics.size() - 1; i++) {
                expectedParameters.push_back(lambda->generics[i]);
            }
        } else {
            // Get function
            BalanceFunction *bfunction = currentPackage->currentModule->getFunction(functionName);
            if (bfunction == nullptr) {
                // Check if imported
                BalanceImportedFunction *ibfunction = currentPackage->currentModule->getImportedFunction(functionName);
                if (ibfunction == nullptr) {
                    currentPackage->currentModule->addTypeError(ctx, "Unknown function: " + functionName);
                    return new BalanceTypeString("Unknown");
                } else {
                    bfunction = ibfunction->bfunction;
                }
            }

            returnType = bfunction->returnTypeString;

            // Check call arguments match
            for (BalanceParameter * parameter : bfunction->parameters) {
                expectedParameters.push_back(parameter->balanceTypeString);
            }
        }
    }

    // Temporary hack until we have a good way to store builtins
    if (functionName == "print" || functionName == "open") {
        return returnType;
    }

    // Parse arguments
    for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
        BalanceTypeString * typeString = any_cast<BalanceTypeString *>(visit(argument));
        actualParameters.push_back(typeString);
    }

    for (int i = 0; i < expectedParameters.size(); i++) {
        if (i >= actualParameters.size()) {
            currentPackage->currentModule->addTypeError(ctx, "Missing parameter in call, " + text + ", expected " + expectedParameters[i]->toString());
            continue;
        }

        if (!expectedParameters[i]->equalTo(actualParameters[i])) {
            currentPackage->currentModule->addTypeError(ctx, "Incorrect parameter type. Expected " + expectedParameters[i]->toString() + ", found " + actualParameters[i]->toString());
        }
    }

    for (int i = expectedParameters.size(); i < actualParameters.size(); i++) {
        currentPackage->currentModule->addTypeError(ctx, "Extraneous parameter in call, " + text + ", found " + actualParameters[i]->toString());
    }

    return returnType;
}

std::any TypeVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) {
    std::string functionName = ctx->IDENTIFIER()->getText();
    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, scope);

    BalanceFunction *bfunction;

    if (currentPackage->currentModule->currentClass != nullptr) {
        bfunction = currentPackage->currentModule->currentClass->methods[functionName];
    } else {
        bfunction = currentPackage->currentModule->getFunction(functionName);
    }

    for (BalanceParameter * parameter : bfunction->parameters) {
        currentPackage->currentModule->currentScope->typeSymbolTable[parameter->name] = parameter->balanceTypeString;
    }

    currentPackage->currentModule->currentFunction = bfunction;

    visit(ctx->functionBlock());

    if (currentPackage->currentModule->currentFunction->returnTypeString->base != "None" && !currentPackage->currentModule->currentFunction->hasExplicitReturn) {
        currentPackage->currentModule->addTypeError(ctx, "Missing return statement");
    }

    currentPackage->currentModule->currentFunction = nullptr;
    currentPackage->currentModule->currentScope = scope;
    return std::any();
}

std::any TypeVisitor::visitClassInitializer(BalanceParser::ClassInitializerContext *ctx) {
    BalanceTypeString * className = new BalanceTypeString(ctx->IDENTIFIER()->getText());

    BalanceClass * bclass = currentPackage->currentModule->getClass(className);
    if (bclass == nullptr) {
        BalanceImportedClass * ibclass = currentPackage->currentModule->getImportedClass(className);
        if (ibclass == nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Unknown class name: " + className->toString());
        }
    }

    // TODO: When we introduce constructors and other initializers, add type-checking here.

    return className;
}

std::any TypeVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string className = ctx->className->getText();
    string text = ctx->getText();

    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, scope);

    BalanceTypeString * btypestring = new BalanceTypeString(className); // TODO: Clean this up

    BalanceClass * bclass = currentPackage->currentModule->getClass(btypestring);
    if (bclass == nullptr) {
        BalanceImportedClass * ibclass = currentPackage->currentModule->getImportedClass(btypestring);
        if (ibclass == nullptr) {
            // TODO: Handle
        }
    }

    currentPackage->currentModule->currentClass = bclass;
    currentPackage->currentModule->classes[className] = bclass;

    // Visit all class functions
    for (auto const &x : ctx->classElement()) {
        if (x->functionDefinition()) {
            visit(x);
        }
    }

    currentPackage->currentModule->currentClass = nullptr;
    currentPackage->currentModule->currentScope = scope;
    return std::any();
}

std::any TypeVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    string text = ctx->getText();
    // TODO: Everywhere we new up BalanceTypeString directly from text, we need a string -> BalanceTypeString converter that takes care of generics
    BalanceTypeString * typeString = new BalanceTypeString(ctx->type->getText());
    string name = ctx->name->getText();
    currentPackage->currentModule->currentScope->typeSymbolTable[name] = typeString;
    return std::any();
}

std::any TypeVisitor::visitLambda(BalanceParser::LambdaContext *ctx) {
    std::string text = ctx->getText();
    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;

    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, scope);
    vector<BalanceParameter *> functionParameterTypes;
    for (BalanceParser::ParameterContext *parameter : ctx->parameterList()->parameter()) {
        string parameterName = parameter->identifier->getText();
        BalanceTypeString * typeString = new BalanceTypeString(parameter->type->getText());
        functionParameterTypes.push_back(new BalanceParameter(typeString, parameterName));
        currentPackage->currentModule->currentScope->typeSymbolTable[parameterName] = typeString;
    }

    // If we don't have a return type, assume none
    BalanceTypeString *returnType;
    if (ctx->returnType()) {
        std::string functionReturnTypeString = ctx->returnType()->balanceType()->getText();
        returnType = new BalanceTypeString(functionReturnTypeString);
    } else {
        returnType = new BalanceTypeString("None");
    }

    currentPackage->currentModule->currentLambda = new BalanceLambda(functionParameterTypes, returnType);

    visit(ctx->functionBlock());

    if (currentPackage->currentModule->currentLambda->returnTypeString->base != "None" && !currentPackage->currentModule->currentLambda->hasExplicitReturn) {
        currentPackage->currentModule->addTypeError(ctx, "Missing return statement");
    }

    currentPackage->currentModule->currentScope = scope;

    // TODO: Test nested lambdas - we might need this to be a stack
    currentPackage->currentModule->currentLambda = nullptr;

    vector<BalanceTypeString *> parameters;
    for (BalanceParameter * p : functionParameterTypes) {
        parameters.push_back(p->balanceTypeString);
    }
    parameters.push_back(returnType);
    return new BalanceTypeString("Lambda", parameters);
}

// std::any TypeVisitor::visitReturnType(BalanceParser::ReturnTypeContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitBlock(BalanceParser::BlockContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitFunctionBlock(BalanceParser::FunctionBlockContext *ctx) { return std::any(); }
// std::any TypeVisitor::visitLiteral(BalanceParser::LiteralContext *ctx) { return std::any(); }
std::any TypeVisitor::visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) {
    string text = ctx->getText();

    BalanceTypeString * firstArrayValue = nullptr;
    for (BalanceParser::ExpressionContext *expression : ctx->listElements()->expression()) {
        BalanceTypeString * typeString = any_cast<BalanceTypeString *>(visit(expression));
        if (firstArrayValue == nullptr) {
            firstArrayValue = typeString;
            continue;
        }

        if (!firstArrayValue->equalTo(typeString)) {
            currentPackage->currentModule->addTypeError(ctx, "Array types differ. Expected " + firstArrayValue->toString() + ", found " + typeString->toString());
        }
    }

    return new BalanceTypeString("Array", { firstArrayValue });
}

std::any TypeVisitor::visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) {
    return new BalanceTypeString("Int");
}

std::any TypeVisitor::visitStringLiteral(BalanceParser::StringLiteralContext *ctx) {
    return new BalanceTypeString("String");
}

std::any TypeVisitor::visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) {
    return new BalanceTypeString("Bool");
}

std::any TypeVisitor::visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) {
    return new BalanceTypeString("Double");
}

// std::any TypeVisitor::visitSimpleType(BalanceParser::SimpleTypeContext *ctx) {
//     if (ctx->IDENTIFIER()) {
//         return new BalanceTypeString(ctx->IDENTIFIER()->getText());
//     } else {
//         return new BalanceTypeString(ctx->NONE()->getText());
//     }
// }

std::any TypeVisitor::visitNoneLiteral(BalanceParser::NoneLiteralContext *ctx) {
    return new BalanceTypeString("None");
}
