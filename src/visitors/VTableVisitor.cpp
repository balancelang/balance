#include "VTableVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::any VTableVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    // TODO: Do we need this? Can we do it in Utilities::createDefaultConstructor ?
    std::string text = ctx->getText();
    string className = ctx->className->getText();

    BalanceTypeString * btypeString = new BalanceTypeString(className);

    BalanceClass *bclass = currentPackage->currentModule->getClass(btypeString);

    StructType *vTableStructType = StructType::create(*currentPackage->context, className + "_vtable");
    bclass->vTableStructType = vTableStructType;
    std::vector<Type *> functions = {};

    for (auto const &x : bclass->getMethods()) {
        BalanceFunction *bfunction = x.second;
        functions.push_back(bfunction->function->getType());
    }

    ArrayRef<Type *> propertyTypesRef(functions);
    vTableStructType->setBody(propertyTypesRef, false);

    // Build a new vector of the old struct-elements, and push the vtable pointer
    vector<Type *> types;
    for (auto x : bclass->structType->elements()) {
        types.push_back(x);
    }
    bclass->vtableTypeIndex = types.size();
    types.push_back(vTableStructType->getPointerTo());
    ArrayRef<Type *> vTableArrayRef(types);
    bclass->structType->setBody(vTableArrayRef);

    return std::any();
}