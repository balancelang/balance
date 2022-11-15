#include "VTableVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::any VTableVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    std::string text = ctx->getText();
    string className = ctx->className->getText();

    BalanceTypeString * btypeString = new BalanceTypeString(className);

    BalanceClass *bclass = currentPackage->currentModule->getClass(btypeString);

    StructType *structType = StructType::create(*currentPackage->context, className + "_vtable");

    std::vector<Type *> functions = {};

    for (auto const &x : bclass->getMethods()) {
        BalanceFunction *bfunction = x.second;
        functions.push_back(bfunction->function->getType());
    }

    ArrayRef<Type *> propertyTypesRef(functions);
    structType->setBody(propertyTypesRef, false);

    return std::any();
}