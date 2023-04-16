#include "ClassVTableVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::any ClassVTableVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    std::string text = ctx->getText();
    string className = ctx->className->getText();

    // For each known generic use of class, visit the class
    std::vector<BalanceType *> btypes = {};

    if (ctx->classGenerics()) {
        btypes = currentPackage->currentModule->getGenericVariants(className);
    } else {
        BalanceType * btype = currentPackage->currentModule->getType(className);
        btypes.push_back(btype);
    }

    for (BalanceType * btype : btypes) {
        // For each interface implemented by class, define this class' implementation of that interface's vtable type
        for (auto const &x : btype->interfaces) {
            BalanceType * binterface = x.second;

            vector<Constant *> values;
            for (BalanceFunction * bfunction : binterface->getMethods()) {
                Function * function = btype->getMethod(bfunction->name)->getFunctionDefinition();
                values.push_back(ConstantExpr::getBitCast(function, bfunction->getFunctionDefinition()->getType()));
            }

            std::string name = className + "_" + binterface->toString() + "_vtable";
            ArrayRef<Constant *> valuesRef(values);
            Constant * vTableData = ConstantStruct::get(binterface->vTableStructType, valuesRef);
            GlobalVariable * vTableDataVariable = new GlobalVariable(*currentPackage->currentModule->module, binterface->vTableStructType, true, GlobalValue::ExternalLinkage, vTableData, name);
            btype->interfaceVTables[x.first] = vTableDataVariable;
        }
    }

    return std::any();
}
