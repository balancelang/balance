#include "InheritanceVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::any InheritanceVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string className = ctx->className->getText();

    BalanceType * btype = currentPackage->currentModule->getType(className);
    currentPackage->currentModule->currentType = btype;

    visit(ctx->classExtendsImplements());

    currentPackage->currentModule->currentType = nullptr;
    return nullptr;
}

std::any InheritanceVisitor::visitClassExtendsImplements(BalanceParser::ClassExtendsImplementsContext *ctx) {
    string text = ctx->getText();

    if (currentPackage->currentModule->currentType == nullptr) {
        throw std::runtime_error("Can't visit extends/implements when not parsing class.");
    }

    if (ctx->extendedClass) {
        BalanceParser::BalanceTypeContext *type = ctx->extendedClass;

        std::string className = type->getText();
        BalanceType * btype = currentPackage->currentModule->getType(className);
        if (btype == nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Unknown class: " + className);
            return nullptr;
        }

        if (btype->isInterface) {
            currentPackage->currentModule->addTypeError(ctx, "Can't extend interfaces - did you mean 'implements'? Interface: " + className);
            return nullptr;
        }

        currentPackage->currentModule->currentType->addParent(btype);
    }

    return nullptr;
}