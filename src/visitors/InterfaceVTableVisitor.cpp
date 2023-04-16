#include "InterfaceVTableVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::any InterfaceVTableVisitor::visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) {
    std::string text = ctx->getText();
    string interfaceName = ctx->interfaceName->getText();

    BalanceType * binterface = currentPackage->currentModule->getType(interfaceName);

    StructType *vTableStructType = StructType::create(*currentPackage->context, interfaceName + "_vtable");
    binterface->vTableStructType = vTableStructType;
    std::vector<Type *> functions = {};

    for (BalanceFunction *bfunction : binterface->getMethods()) {
        functions.push_back(bfunction->getFunctionDefinition()->getType());
        // TODO: Store vtableIndex for each function here?
    }

    ArrayRef<Type *> propertyTypesRef(functions);
    vTableStructType->setBody(propertyTypesRef, false);

    return std::any();
}