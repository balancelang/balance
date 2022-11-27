#include "InterfaceVTableVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::any InterfaceVTableVisitor::visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) {
    std::string text = ctx->getText();
    string interfaceName = ctx->interfaceName->getText();

    BalanceTypeString * btypeString = new BalanceTypeString(interfaceName);

    BalanceInterface *binterface = currentPackage->currentModule->getInterface(btypeString->base);

    StructType *vTableStructType = StructType::create(*currentPackage->context, interfaceName + "_vtable");
    binterface->vTableStructType = vTableStructType;
    std::vector<Type *> functions = {};

    for (auto const &x : binterface->getMethods()) {
        BalanceFunction *bfunction = x.second;
        functions.push_back(bfunction->function->getType());
        // TODO: Store vtableIndex for each function here?
    }

    ArrayRef<Type *> propertyTypesRef(functions);
    vTableStructType->setBody(propertyTypesRef, false);

    return std::any();
}