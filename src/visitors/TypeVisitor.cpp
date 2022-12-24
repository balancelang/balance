#include "TypeVisitor.h"
#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

// Returns whether a can be assigned to b
bool canAssignTo(ParserRuleContext * ctx, BalanceType * aType, BalanceType * bType) {
    if (aType->name == "Lambda" || bType->name == "Lambda") {
        // Short-term hack for now.
        return true;
    }

    // If a is interface, it can never be assigned to something not interface
    if (!bType->isInterface && aType->isInterface) {
        currentPackage->currentModule->addTypeError(ctx, "Can't assign interface to non-interface");
        return false;
    }

    // If b is interface and a is not, check that a implements b
    if (bType->isInterface && !aType->isInterface) {
        if (aType->interfaces[bType->name] == nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Type " + aType->toString() + " does not implement interface " + bType->toString());
            return false;
        }
        return true;
    }

    // Do similar checks for inheritance, once implemented

    if (!aType->equalTo(bType)) {
        currentPackage->currentModule->addTypeError(ctx, "Types are not equal in member assignment. Expected " + bType->toString() + ", found " + aType->toString());
        return false;
    }

    return true;
}

std::string TypeVisitor::getText(antlr4::ParserRuleContext *ctx) {
    int a = ctx->start->getStartIndex();
    int b = ctx->stop->getStopIndex();
    // ctx->getSourceInterval()  TODO: Maybe we can use getSourceInterval here?
    antlr4::misc::Interval * interval = new antlr4::misc::Interval((size_t) a, (size_t) b);
    return currentPackage->currentModule->antlrStream->getText(*interval);
}

std::any TypeVisitor::visitWhileStatement(BalanceParser::WhileStatementContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;

    BalanceType * condition = any_cast<BalanceType *>(visit(ctx->expression()));
    if (condition->name != "Bool") {
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

    BalanceType * condition = any_cast<BalanceType *>(visit(ctx->expression()));
    if (condition->name != "Bool") {
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

    BalanceType * returnValue = any_cast<BalanceType *>(visit(ctx->expression()));

    if (currentPackage->currentModule->currentFunction != nullptr) {
        if (!canAssignTo(ctx, returnValue, currentPackage->currentModule->currentFunction->returnType)) {
            currentPackage->currentModule->addTypeError(ctx, "Return types differ. Expected " + currentPackage->currentModule->currentFunction->returnType->toString() + ", found " + returnValue->toString());
        }
        currentPackage->currentModule->currentFunction->hasExplicitReturn = true;
    }
    if (currentPackage->currentModule->currentLambda != nullptr) {
        if (!canAssignTo(ctx, returnValue, currentPackage->currentModule->currentLambda->returnType)) {
            currentPackage->currentModule->addTypeError(ctx, "Return types differ. Expected " + currentPackage->currentModule->currentLambda->returnType->toString() + ", found " + returnValue->toString());
        }
        currentPackage->currentModule->currentLambda->hasExplicitReturn = true;
    }

    return std::any();
}

std::any TypeVisitor::visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) {
    std::string text = this->getText(ctx);
    std::string variableName = ctx->IDENTIFIER()->getText();

    BalanceValue *tryVal = currentPackage->currentModule->currentScope->symbolTable[variableName];
    if (tryVal != nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Duplicate variable name: " + variableName);
        return std::any();
    }

    BalanceType * value = any_cast<BalanceType *>(visit(ctx->expression()));

    currentPackage->currentModule->currentScope->symbolTable[variableName] = new BalanceValue(value, nullptr);

    return std::any();
}

std::any TypeVisitor::visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) {
    std::string text = this->getText(ctx);
    std::string variableName = ctx->IDENTIFIER()->getText();
    BalanceValue *tryVal = currentPackage->currentModule->getValue(variableName);
    if (tryVal == nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Unknown variable: " + variableName);
        return currentPackage->currentModule->getType("Unknown");
    }

    BalanceType * expression = any_cast<BalanceType *>(visit(ctx->expression()));
    if (!tryVal->type->equalTo(expression)) {
        currentPackage->currentModule->addTypeError(ctx, "Types are not equal in reassignment: " + text + ", " + tryVal->type->toString() + " != " + expression->toString());
    }

    return std::any();
}

std::any TypeVisitor::visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) {
    std::string text = ctx->getText();

    BalanceType * member = any_cast<BalanceType *>(visit(ctx->member));
    BalanceType * expression = any_cast<BalanceType *>(visit(ctx->value));

    if (ctx->index) {
        BalanceType * index = any_cast<BalanceType *>(visit(ctx->index));
        if (member->name == "Array" && index->name != "Int") {
            currentPackage->currentModule->addTypeError(ctx, "Array index must be Int: " + text + ", found " + index->toString());
        }

        if (member->name == "Array" && !canAssignTo(ctx, expression, member->generics[0])) {
            currentPackage->currentModule->addTypeError(ctx, "Types are not equal in array assignment. Expected " + member->generics[0]->toString() + ", found " + expression->toString());
        }
        // TODO: Handle
    } else {
        std::string accessName = ctx->access->getText();

        BalanceProperty * bproperty = member->getProperty(accessName);
        if (bproperty == nullptr) {
            // Unknown class property
            currentPackage->currentModule->addTypeError(ctx, "Unknown property. Type " + member->toString() + " does not have a property named " + accessName);
            return std::any();
        }

        // Check if expression can be assigned to property type, has the side-effect of reporting typeError
        canAssignTo(ctx, expression, bproperty->balanceType);
    }

    return std::any();
}

std::any TypeVisitor::visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) {
    std::string text = ctx->getText();

    BalanceType * accessedType = any_cast<BalanceType *>(visit(ctx->member));

    // Store this value, so that visiting the access will use it as context
    currentPackage->currentModule->accessedType = accessedType;

    BalanceType *returnType = any_cast<BalanceType *>(visit(ctx->access));
    currentPackage->currentModule->accessedType = nullptr;
    return returnType;
}

std::any TypeVisitor::visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceType * lhs = any_cast<BalanceType *>(visit(ctx->lhs));
    BalanceType * rhs = any_cast<BalanceType *>(visit(ctx->rhs));

    if (!lhs->equalTo(rhs)) {
        currentPackage->currentModule->addTypeError(ctx, "Types are not equal in additive expression: " + text + ", " + lhs->toString() + " != " + rhs->toString());
    }

    return lhs;
}

std::any TypeVisitor::visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceType * lhs = any_cast<BalanceType *>(visit(ctx->lhs));
    BalanceType * rhs = any_cast<BalanceType *>(visit(ctx->rhs));

    if (!lhs->equalTo(rhs)) {
        currentPackage->currentModule->addTypeError(ctx, "Types are not equal in relational expression: " + text + ", " + lhs->toString() + " != " + rhs->toString());
    }

    return currentPackage->currentModule->getType("Bool");
}

std::any TypeVisitor::visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceType * member = any_cast<BalanceType *>(visit(ctx->member));
    BalanceType * index = any_cast<BalanceType *>(visit(ctx->index));

    if (member->name == "Array" && index->name != "Int") {
        currentPackage->currentModule->addTypeError(ctx, "Array index must be Int: " + text + ", found " + index->toString());
    }

    // TODO: When we introduce Dictionaries, it will probably be return member->generics[1]
    // TODO: Throw error for now

    // This works because accessing arrays returns the 0th generic type
    return member->generics[0];
}

std::any TypeVisitor::visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceType * lhs = any_cast<BalanceType *>(visit(ctx->lhs));
    BalanceType * rhs = any_cast<BalanceType *>(visit(ctx->rhs));

    if (!lhs->equalTo(rhs)) {
        currentPackage->currentModule->addTypeError(ctx, "Types are not equal in multiplicative expression: " + text + ", " + lhs->toString() + " != " + rhs->toString());
    }

    // TODO: How can we define which types can be multiplied? e.g. "abc" * "abc" ?

    return lhs;
}

std::any TypeVisitor::visitVariable(BalanceParser::VariableContext *ctx) {
    std::string variableName = ctx->IDENTIFIER()->getText();

    // Check if we're accessing a type property
    if (currentPackage->currentModule->accessedType != nullptr) {
        BalanceType * btype = currentPackage->currentModule->getType(currentPackage->currentModule->accessedType->name);
        if (btype == nullptr) {
            return currentPackage->currentModule->getType("Unknown");
        }

        BalanceProperty * bproperty = btype->getProperty(variableName);
        if (bproperty == nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Unknown class property " + variableName + ", on type " + btype->toString());
            return currentPackage->currentModule->getType("Unknown");
        }

        return bproperty->balanceType;
    }

    BalanceValue *tryVal = currentPackage->currentModule->getValue(variableName);
    if (tryVal != nullptr) {
        return tryVal->type;
    }

    currentPackage->currentModule->addTypeError(ctx, "Unknown variable: " + variableName);
    return currentPackage->currentModule->getType("Unknown");
}

std::any TypeVisitor::visitGenericType(BalanceParser::GenericTypeContext *ctx) {
    std::string base = ctx->base->getText();
    std::vector<BalanceType *> generics;

    for (BalanceParser::BalanceTypeContext *type: ctx->typeList()->balanceType()) {
        BalanceType * btype = any_cast<BalanceType *>(visit(type));
        generics.push_back(btype);
    }

    // TODO: Figure out how we test if the class supports the generics

    return currentPackage->currentModule->getType(base, generics);
}

std::any TypeVisitor::visitFunctionCall(BalanceParser::FunctionCallContext *ctx) {
    std::string text = this->getText(ctx);
    std::string functionName = ctx->IDENTIFIER()->getText();

    BalanceType * returnType = nullptr;
    vector<BalanceType *> expectedParameters;
    vector<BalanceType *> actualParameters;

    // Check if we are accessing a type
    if (currentPackage->currentModule->accessedType != nullptr) {
        // check if function exists
        BalanceFunction * bfunction = currentPackage->currentModule->accessedType->getMethod(functionName);
        if (bfunction == nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Unknown method. Type " + currentPackage->currentModule->accessedType->toString() + " does not have a method called " + functionName);
            return currentPackage->currentModule->getType("Unknown");
        }
        returnType = bfunction->returnType;

        // TODO: Remove 'this'
        for (int i = 1; i < bfunction->parameters.size(); i++) {
            expectedParameters.push_back(bfunction->parameters[i]->balanceType);
        }
    } else {
        BalanceValue * lambda = currentPackage->currentModule->getValue(functionName);

        // Check if we have a lambda in scope, else it must be a function
        if (lambda != nullptr) {
            returnType = lambda->type->generics.back();

            for (int i = 0; i < lambda->type->generics.size() - 1; i++) {
                expectedParameters.push_back(lambda->type->generics[i]);
            }
        } else {
            // Get function
            BalanceFunction *bfunction = currentPackage->currentModule->getFunction(functionName);
            if (bfunction == nullptr) {
                currentPackage->currentModule->addTypeError(ctx, "Unknown function: " + functionName);
                return currentPackage->currentModule->getType("Unknown");
            }

            returnType = bfunction->returnType;

            // Check call arguments match
            for (BalanceParameter * parameter : bfunction->parameters) {
                expectedParameters.push_back(parameter->balanceType);
            }
        }
    }

    // Don't consider accessedType when parsing arguments
    BalanceType * backup = currentPackage->currentModule->accessedType;
    currentPackage->currentModule->accessedType = nullptr;

    // Parse arguments
    for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
        BalanceType * btype = any_cast<BalanceType *>(visit(argument));
        actualParameters.push_back(btype);
    }
    currentPackage->currentModule->accessedType = backup;

    // Temporary hack until we have a good way to store builtins
    if (functionName == "print" || functionName == "open") {
        return returnType;
    }

    for (int i = 0; i < expectedParameters.size(); i++) {
        if (i >= actualParameters.size()) {
            currentPackage->currentModule->addTypeError(ctx, "Missing parameter in call, " + text + ", expected " + expectedParameters[i]->toString());
            continue;
        }

        if (!expectedParameters[i]->equalTo(actualParameters[i])) {
            BalanceType * expectedType = expectedParameters[i];
            BalanceType * actualType = actualParameters[i];

            if (actualType == nullptr) {
                currentPackage->currentModule->addTypeError(ctx, "Unknown parameter type. Expected " + expectedType->toString() + ", found: " + actualType->toString());
            }

            // Check if we're expecting interface and type doesn't implement
            if (expectedType->isInterface) {
                // TODO: What if actualType is interface?
                if (actualType->isInterface) {
                    // TODO: Check if interface is same
                } else {
                    if (actualType->interfaces[expectedType->name] == nullptr) {
                        currentPackage->currentModule->addTypeError(ctx, "Incorrect parameter type. " + actualType->toString() + " does not implement interface " + expectedType->toString());
                    }
                }
            } else {
                currentPackage->currentModule->addTypeError(ctx, "Incorrect parameter type. Expected " + expectedType->toString() + ", found: " + actualType->toString());
            }
        }
    }

    for (int i = expectedParameters.size(); i < actualParameters.size(); i++) {
        currentPackage->currentModule->addTypeError(ctx, "Extraneous parameter in call, found " + actualParameters[i]->toString());
    }

    return returnType;
}

std::any TypeVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) {
    std::string functionName = ctx->functionSignature()->IDENTIFIER()->getText();
    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, scope);

    BalanceFunction *bfunction;

    if (currentPackage->currentModule->currentType != nullptr) {
        bfunction = currentPackage->currentModule->currentType->getMethod(functionName);
    } else {
        bfunction = currentPackage->currentModule->getFunction(functionName);
    }

    for (BalanceParameter * parameter : bfunction->parameters) {
        currentPackage->currentModule->currentScope->symbolTable[parameter->name] = new BalanceValue(parameter->balanceType, nullptr);
    }

    currentPackage->currentModule->currentFunction = bfunction;

    visit(ctx->functionBlock());

    if (currentPackage->currentModule->currentFunction->returnType->name != "None" && !currentPackage->currentModule->currentFunction->hasExplicitReturn) {
        currentPackage->currentModule->addTypeError(ctx, "Missing return statement");
    }

    currentPackage->currentModule->currentFunction = nullptr;
    currentPackage->currentModule->currentScope = scope;
    return std::any();
}

std::any TypeVisitor::visitClassInitializer(BalanceParser::ClassInitializerContext *ctx) {
    std::string className = ctx->IDENTIFIER()->getText();
    BalanceType * btype = currentPackage->currentModule->getType(className);

    if (btype == nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Unknown type: " + className);
    }

    if (btype->isInterface) {
        currentPackage->currentModule->addTypeError(ctx, "Can't initialize interfaces: " + className);
    }

    if (btype->isSimpleType) {
        currentPackage->currentModule->addTypeError(ctx, "Can't initialize simple types: " + className);
    }

    // TODO: When we introduce constructors and other initializers, add type-checking here.

    return btype;
}

std::any TypeVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string className = ctx->className->getText();
    string text = ctx->getText();

    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;
    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, scope);

    BalanceType * btype = currentPackage->currentModule->getType(className);
    if (btype == nullptr) {
        // TODO: Handle
    }

    currentPackage->currentModule->currentType = btype;

    // Parse extend/implements
    if (ctx->classExtendsImplements()) {
        visit(ctx->classExtendsImplements());
    }

    // Visit all class properties
    for (auto const &x : ctx->classElement()) {
        if (x->classProperty()) {
            visit(x);
        }
    }

    // Visit all class functions
    for (auto const &x : ctx->classElement()) {
        if (x->functionDefinition()) {
            visit(x);
        }
    }

    currentPackage->currentModule->currentType = nullptr;
    currentPackage->currentModule->currentScope = scope;
    return std::any();
}

std::any TypeVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    string text = ctx->getText();
    string typeName = ctx->type->getText();
    string propertyName = ctx->name->getText();
    // TODO: Parse generics
    BalanceProperty * bproperty = currentPackage->currentModule->currentType->properties[propertyName];

    BalanceType * btype = currentPackage->currentModule->getType(typeName);
    if (btype == nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Unknown class property type");
    }

    string name = ctx->name->getText();
    currentPackage->currentModule->currentScope->symbolTable[name] = new BalanceValue(bproperty->balanceType, nullptr);
    return std::any();
}

std::any TypeVisitor::visitLambda(BalanceParser::LambdaContext *ctx) {
    std::string text = ctx->getText();
    BalanceScopeBlock *scope = currentPackage->currentModule->currentScope;

    currentPackage->currentModule->currentScope = new BalanceScopeBlock(nullptr, scope);
    vector<BalanceParameter *> functionParameterTypes;
    for (BalanceParser::ParameterContext *parameter : ctx->parameterList()->parameter()) {
        string parameterName = parameter->identifier->getText();
        BalanceType * btype = currentPackage->currentModule->getType(parameter->type->getText());
        functionParameterTypes.push_back(new BalanceParameter(btype, parameterName));
        currentPackage->currentModule->currentScope->symbolTable[parameterName] = new BalanceValue(btype, nullptr);
    }

    // If we don't have a return type, assume none
    BalanceType *returnType;
    if (ctx->returnType()) {
        std::string functionReturnTypeString = ctx->returnType()->balanceType()->getText();
        returnType = currentPackage->currentModule->getType(functionReturnTypeString);
    } else {
        returnType = currentPackage->currentModule->getType("None");
    }

    currentPackage->currentModule->currentLambda = new BalanceLambda(functionParameterTypes, returnType);

    visit(ctx->functionBlock());

    if (currentPackage->currentModule->currentLambda->returnType->name != "None" && !currentPackage->currentModule->currentLambda->hasExplicitReturn) {
        currentPackage->currentModule->addTypeError(ctx, "Missing return statement");
    }

    currentPackage->currentModule->currentScope = scope;

    // TODO: Test nested lambdas - we might need this to be a stack
    currentPackage->currentModule->currentLambda = nullptr;

    vector<BalanceType *> parameters;
    for (BalanceParameter * p : functionParameterTypes) {
        parameters.push_back(p->balanceType);
    }
    parameters.push_back(returnType);
    return currentPackage->currentModule->getType("Lambda", parameters);
}

std::any TypeVisitor::visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) {
    string text = ctx->getText();

    BalanceType * firstArrayValue = nullptr;
    for (BalanceParser::ExpressionContext *expression : ctx->listElements()->expression()) {
        BalanceType * typeString = any_cast<BalanceType *>(visit(expression));
        if (firstArrayValue == nullptr) {
            firstArrayValue = typeString;
            continue;
        }

        if (!firstArrayValue->equalTo(typeString)) {
            currentPackage->currentModule->addTypeError(ctx, "Array types differ. Expected " + firstArrayValue->toString() + ", found " + typeString->toString());
        }
    }

    return currentPackage->currentModule->getType("Array", { firstArrayValue });
}

std::any TypeVisitor::visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) {
    return currentPackage->currentModule->getType("Int");
}

std::any TypeVisitor::visitStringLiteral(BalanceParser::StringLiteralContext *ctx) {
    return currentPackage->currentModule->getType("String");
}

std::any TypeVisitor::visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) {
    return currentPackage->currentModule->getType("Bool");
}

std::any TypeVisitor::visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) {
    return currentPackage->currentModule->getType("Double");
}

std::any TypeVisitor::visitNoneLiteral(BalanceParser::NoneLiteralContext *ctx) {
    return currentPackage->currentModule->getType("None");
}

std::any TypeVisitor::visitClassExtendsImplements(BalanceParser::ClassExtendsImplementsContext *ctx) {
    string text = ctx->getText();

    if (currentPackage->currentModule->currentType == nullptr) {
        throw std::runtime_error("Can't visit extends/implements when not parsing class.");
    }

    for (BalanceParser::BalanceTypeContext *type: ctx->interfaces->balanceType()) {
        std::string interfaceName = type->getText();
        BalanceType * btype = currentPackage->currentModule->getType(interfaceName);
        if (btype == nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Unknown interface: " + interfaceName);
            continue;
        }

        if (!btype->isInterface) {
            currentPackage->currentModule->addTypeError(ctx, "This is not an interface: " + interfaceName);
            continue;
        }

        // Check if class implements all functions
        for (auto const &x : btype->getMethods()) {
            BalanceFunction * interfaceFunction = x.second;

            // Check if class implements function
            BalanceFunction * classFunction = currentPackage->currentModule->currentType->getMethod(interfaceFunction->name);
            if (classFunction == nullptr) {
                currentPackage->currentModule->addTypeError(ctx, "Missing interface implementation of function " + interfaceFunction->name + ", required by interface " + btype->toString());
                continue;
            }

            // Check if signature matches
            if (!interfaceFunction->returnType->equalTo(classFunction->returnType)) {
                currentPackage->currentModule->addTypeError(ctx, "Wrong return-type of implemented function. Expected " + interfaceFunction->returnType->toString() + ", found " + classFunction->returnType->toString());
            }

            // We start from 1, since we assume 'this' as first parameter
            for (int i = 1; i < interfaceFunction->parameters.size(); i++) {
                if (i >= classFunction->parameters.size()) {
                    currentPackage->currentModule->addTypeError(ctx, "Missing parameter in implemented method, " + text + ", expected " + interfaceFunction->parameters[i]->balanceType->toString());
                    continue;
                }

                if (!interfaceFunction->parameters[i]->balanceType->equalTo(classFunction->parameters[i]->balanceType)) {
                    currentPackage->currentModule->addTypeError(ctx, "Incorrect parameter type. Expected " + interfaceFunction->parameters[i]->balanceType->toString() + ", found " + classFunction->parameters[i]->balanceType->toString());
                }
            }

            for (int i = interfaceFunction->parameters.size(); i < classFunction->parameters.size(); i++) {
                currentPackage->currentModule->addTypeError(ctx, "Extraneous parameter in implemented method, " + classFunction->name + ", found " + classFunction->parameters[i]->balanceType->toString());
            }
        }

        currentPackage->currentModule->currentType->interfaces[interfaceName] = btype;
    }

    return nullptr;
}