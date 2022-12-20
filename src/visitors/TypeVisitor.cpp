#include "TypeVisitor.h"
#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

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

std::any TypeVisitor::visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) {
    std::string text = ctx->getText();

    BalanceTypeString * member = any_cast<BalanceTypeString *>(visit(ctx->member));
    BalanceTypeString * expression = any_cast<BalanceTypeString *>(visit(ctx->value));

    if (ctx->index) {
        BalanceTypeString * index = any_cast<BalanceTypeString *>(visit(ctx->index));
        if (member->base == "Array" && index->base != "Int") {
            currentPackage->currentModule->addTypeError(ctx, "Array index must be Int: " + text + ", found " + index->toString());
        }

        if (member->base == "Array" && !member->generics[0]->equalTo(expression)) {
            currentPackage->currentModule->addTypeError(ctx, "Types are not equal in array assignment. Expected " + member->generics[0]->toString() + ", found " + expression->toString());
        }
        // TODO: Handle
    } else {
        std::string accessName = ctx->access->getText();
        BalanceType * btype = currentPackage->currentModule->getType(member);
        if (btype == nullptr) {
            // TODO: Handle
        }

        BalanceProperty * bproperty = btype->getProperty(accessName);
        if (bproperty == nullptr) {
            // Unknown class property
            currentPackage->currentModule->addTypeError(ctx, "Unknown property. Type " + btype->name->toString() + " does not have a property named " + accessName);
            return std::any();
        }

        // Check if property is interface and expression implements
        BalanceType * bpropertyType = currentPackage->currentModule->getType(bproperty->stringType);
        if (bpropertyType->isInterface()) {
            BalanceType * expressionType = currentPackage->currentModule->getType(expression);
            if (expressionType == nullptr) {
                // TODO: Throw error
            }

            if (expressionType->isInterface()) {
                // TODO: Can this happen?
            } else {
                BalanceClass * expressionClass = (BalanceClass *) expressionType;

                if (expressionClass->interfaces[bproperty->stringType->base] == nullptr) {
                    currentPackage->currentModule->addTypeError(ctx, "Type does not implement interface " + bproperty->stringType->toString());
                }
            }

        } else if (!bproperty->stringType->equalTo(expression)) {
            currentPackage->currentModule->addTypeError(ctx, "Types are not equal in member assignment. Expected " + bproperty->stringType->toString() + ", found " + expression->toString());
        }
    }

    return std::any();
}

std::any TypeVisitor::visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) {
    std::string text = ctx->getText();

    BalanceTypeString * accessedType = any_cast<BalanceTypeString *>(visit(ctx->member));

    // Store this value, so that visiting the access will use it as context
    BalanceType * btype = currentPackage->currentModule->getType(accessedType);
    if (btype == nullptr) {
        // TODO: Handle
    }
    currentPackage->currentModule->accessedType = btype;

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

std::any TypeVisitor::visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceTypeString * member = any_cast<BalanceTypeString *>(visit(ctx->member));
    BalanceTypeString * index = any_cast<BalanceTypeString *>(visit(ctx->index));

    if (member->base == "Array" && index->base != "Int") {
        currentPackage->currentModule->addTypeError(ctx, "Array index must be Int: " + text + ", found " + index->toString());
    }

    // TODO: When we introduce Dictionaries, it will probably be return member->generics[1]
    // TODO: Throw error for now

    // This works because accessing arrays returns the 0th generic type
    return member->generics[0];
}

std::any TypeVisitor::visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) {
    std::string text = this->getText(ctx);

    BalanceTypeString * lhs = any_cast<BalanceTypeString *>(visit(ctx->lhs));
    BalanceTypeString * rhs = any_cast<BalanceTypeString *>(visit(ctx->rhs));

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
            return new BalanceTypeString("Unknown");
        }

        BalanceProperty * bproperty = btype->getProperty(variableName);
        if (bproperty == nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Unknown class property " + variableName + ", on type " + btype->name->toString());
            return new BalanceTypeString("Unknown");
        }

        return bproperty->stringType;
    }

    BalanceTypeString *tryVal = currentPackage->currentModule->getTypeValue(variableName);
    if (tryVal != nullptr) {
        return tryVal;
    }

    currentPackage->currentModule->addTypeError(ctx, "Unknown variable: " + variableName);
    return new BalanceTypeString("Unknown");
}

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

std::any TypeVisitor::visitFunctionCall(BalanceParser::FunctionCallContext *ctx) {
    std::string text = this->getText(ctx);
    std::string functionName = ctx->IDENTIFIER()->getText();

    BalanceTypeString * returnType = nullptr;
    vector<BalanceTypeString *> expectedParameters;
    vector<BalanceTypeString *> actualParameters;

    // Check if we are accessing a type
    if (currentPackage->currentModule->accessedType != nullptr) {
        // check if function exists
        BalanceFunction * bfunction = currentPackage->currentModule->accessedType->getMethod(functionName);
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
                currentPackage->currentModule->addTypeError(ctx, "Unknown function: " + functionName);
                return new BalanceTypeString("Unknown");
            }

            returnType = bfunction->returnTypeString;

            // Check call arguments match
            for (BalanceParameter * parameter : bfunction->parameters) {
                expectedParameters.push_back(parameter->balanceTypeString);
            }
        }
    }

    // Don't consider accessedType when parsing arguments
    BalanceType * backup = currentPackage->currentModule->accessedType;
    currentPackage->currentModule->accessedType = nullptr;

    // Parse arguments
    for (BalanceParser::ArgumentContext *argument : ctx->argumentList()->argument()) {
        BalanceTypeString * typeString = any_cast<BalanceTypeString *>(visit(argument));
        actualParameters.push_back(typeString);
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
            BalanceType * expectedType = currentPackage->currentModule->getType(expectedParameters[i]);
            BalanceType * actualType = currentPackage->currentModule->getType(actualParameters[i]);

            if (actualType == nullptr) {
                currentPackage->currentModule->addTypeError(ctx, "Unknown parameter type. Expected " + expectedType->name->toString() + ", found: " + actualType->name->toString());
            }

            // Check if we're expecting interface and type doesn't implement
            if (expectedType->isInterface()) {
                // TODO: What if actualType is interface?
                if (actualType->isInterface()) {
                    // TODO: Check if interface is same
                } else {
                    BalanceClass * actualClass = (BalanceClass *) actualType;
                    if (actualClass->interfaces[expectedType->name->base] == nullptr) {
                        currentPackage->currentModule->addTypeError(ctx, "Incorrect parameter type. " + actualClass->name->toString() + " does not implement interface " + expectedType->name->toString());
                    }
                }
            } else {
                currentPackage->currentModule->addTypeError(ctx, "Incorrect parameter type. Expected " + expectedType->name->toString() + ", found: " + actualType->name->toString());
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

    if (currentPackage->currentModule->currentClass != nullptr) {
        bfunction = currentPackage->currentModule->currentClass->getMethod(functionName);
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

    BalanceType * btype = currentPackage->currentModule->getType(className);
    if (btype == nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Unknown type: " + className->toString());
    }

    if (btype->isInterface()) {
        currentPackage->currentModule->addTypeError(ctx, "Can't initialize interfaces: " + className->toString());
    }

    if (btype->isSimpleType) {
        currentPackage->currentModule->addTypeError(ctx, "Can't initialize simple types: " + className->toString());
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

    BalanceClass * bclass = (BalanceClass *) currentPackage->currentModule->getType(btypestring);
    if (bclass == nullptr) {
        // TODO: Handle
    }

    currentPackage->currentModule->currentClass = bclass;
    currentPackage->currentModule->classes[className] = bclass;

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

    currentPackage->currentModule->currentClass = nullptr;
    currentPackage->currentModule->currentScope = scope;
    return std::any();
}

std::any TypeVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    string text = ctx->getText();
    string typeName = ctx->type->getText();
    string propertyName = ctx->name->getText();
    // TODO: Parse generics
    BalanceProperty * bproperty = currentPackage->currentModule->currentClass->properties[propertyName];

    BalanceType * btype = currentPackage->currentModule->getType(new BalanceTypeString(typeName));
    if (btype == nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Unknown class property type");
    }

    // // TODO: This is clumsy, where should interface information live?
    // if (btype->isInterface()) {
    //     bproperty->stringType->isInterface = true;
    // }

    string name = ctx->name->getText();
    currentPackage->currentModule->currentScope->typeSymbolTable[name] = bproperty->stringType;
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

std::any TypeVisitor::visitNoneLiteral(BalanceParser::NoneLiteralContext *ctx) {
    return new BalanceTypeString("None");
}

std::any TypeVisitor::visitClassExtendsImplements(BalanceParser::ClassExtendsImplementsContext *ctx) {
    string text = ctx->getText();

    if (currentPackage->currentModule->currentClass == nullptr) {
        throw std::runtime_error("Can't visit extends/implements when not parsing class.");
    }

    for (BalanceParser::BalanceTypeContext *type: ctx->interfaces->balanceType()) {
        std::string interfaceName = type->getText();
        BalanceType * btype = currentPackage->currentModule->getType(new BalanceTypeString(interfaceName));
        if (btype == nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Unknown interface: " + interfaceName);
            continue;
        }

        if (!btype->isInterface()) {
            currentPackage->currentModule->addTypeError(ctx, "This is not an interface: " + interfaceName);
            continue;
        }

        BalanceInterface * binterface = (BalanceInterface *) btype;

        // Check if class implements all functions
        for (auto const &x : binterface->getMethods()) {
            BalanceFunction * interfaceFunction = x.second;

            // Check if class implements function
            BalanceFunction * classFunction = currentPackage->currentModule->currentClass->getMethod(interfaceFunction->name);
            if (classFunction == nullptr) {
                currentPackage->currentModule->addTypeError(ctx, "Missing interface implementation of function " + interfaceFunction->name + ", required by interface " + binterface->name->toString());
                continue;
            }

            // Check if signature matches
            if (!interfaceFunction->returnTypeString->equalTo(classFunction->returnTypeString)) {
                currentPackage->currentModule->addTypeError(ctx, "Wrong return-type of implemented function. Expected " + interfaceFunction->returnTypeString->toString() + ", found " + classFunction->returnTypeString->toString());
            }

            // We start from 1, since we assume 'this' as first parameter
            for (int i = 1; i < interfaceFunction->parameters.size(); i++) {
                if (i >= classFunction->parameters.size()) {
                    currentPackage->currentModule->addTypeError(ctx, "Missing parameter in implemented method, " + text + ", expected " + interfaceFunction->parameters[i]->balanceTypeString->toString());
                    continue;
                }

                if (!interfaceFunction->parameters[i]->balanceTypeString->equalTo(classFunction->parameters[i]->balanceTypeString)) {
                    currentPackage->currentModule->addTypeError(ctx, "Incorrect parameter type. Expected " + interfaceFunction->parameters[i]->balanceTypeString->toString() + ", found " + classFunction->parameters[i]->balanceTypeString->toString());
                }
            }

            for (int i = interfaceFunction->parameters.size(); i < classFunction->parameters.size(); i++) {
                currentPackage->currentModule->addTypeError(ctx, "Extraneous parameter in implemented method, " + classFunction->name + ", found " + classFunction->parameters[i]->balanceTypeString->toString());
            }
        }

        currentPackage->currentModule->currentClass->interfaces[interfaceName] = binterface;
    }

    return nullptr;
}