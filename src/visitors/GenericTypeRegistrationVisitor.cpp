#include "GenericTypeRegistrationVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceType.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"
#include "Visitor.h"

#include "antlr4-runtime.h"
#include "config.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeindex>
#include <typeinfo>

using namespace antlrcpptest;
using namespace std;

extern BalancePackage *currentPackage;

std::any GenericTypeRegistrationVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string className = ctx->className->getText();
    string text = ctx->getText();

    if (ctx->classGenerics()) {
        currentPackage->currentModule->currentType = currentPackage->currentModule->genericTypes[className];
        visit(ctx->classGenerics());
    }

    currentPackage->currentModule->currentType = nullptr;
    return nullptr;
}

std::any GenericTypeRegistrationVisitor::visitClassGenerics(BalanceParser::ClassGenericsContext *ctx) {
    string text = ctx->getText();

    for (BalanceParser::ClassGenericContext * generic : ctx->classGenericsList()->classGeneric()) {
        std::string name = generic->name->getText();
        BalanceType * genericType = new BalanceType(currentPackage->currentModule, name);
        genericType->isGenericPlaceholder = true;
        if (generic->genericCovariance()) {
            std::string secondName = generic->genericCovariance()->IDENTIFIER()->getText();
            if (name != secondName) {
                currentPackage->currentModule->addTypeError(ctx, secondName + " does not exist. Did you mean " + name + "?");
                return nullptr;
            }

            BalanceType * btype = any_cast<BalanceType *>(visit(generic->genericCovariance()->balanceType()));
            genericType->typeRequirements.push_back(btype);
        }

        currentPackage->currentModule->currentType->generics.push_back(genericType);
    }

    return nullptr;
}

std::any GenericTypeRegistrationVisitor::visitGenericType(BalanceParser::GenericTypeContext *ctx) {
    std::string text = ctx->getText();
    std::string className = ctx->IDENTIFIER()->getText();
    std::vector<BalanceType *> generics = {};
    for (BalanceParser::BalanceTypeContext *type : ctx->typeList()->balanceType()) {
        BalanceType * btype = any_cast<BalanceType *>(visit(type));
        generics.push_back(btype);
    }

    BalanceType * btype = currentPackage->currentModule->getType(className, generics);
    if (btype == nullptr) {
        btype = currentPackage->currentModule->createGenericType(className, generics);
    }

    return btype;
}

std::any GenericTypeRegistrationVisitor::visitSimpleType(BalanceParser::SimpleTypeContext *ctx) {
    std::string name = ctx->IDENTIFIER()->getText();
    // Check if it is a class generic
    if (currentPackage->currentModule->currentType != nullptr) {
        for (BalanceType * genericType : currentPackage->currentModule->currentType->generics) {
            if (name == genericType->name) {
                return genericType;
            }
        }
    }
    return currentPackage->currentModule->getType(name);
}

std::any GenericTypeRegistrationVisitor::visitNoneType(BalanceParser::NoneTypeContext *ctx) {
    return currentPackage->currentModule->getType(ctx->NONE()->getText());
}
