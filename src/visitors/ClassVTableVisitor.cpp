#include "ClassVTableVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::any ClassVTableVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    std::string text = ctx->getText();
    string className = ctx->className->getText();

    BalanceType * btype = currentPackage->currentModule->getType(className);

    // For each interface implemented by class, define this class' implementation of that interface's vtable type
    for (auto const &x : btype->interfaces) {
        BalanceType * binterface = x.second;

        vector<Constant *> values;
        for (BalanceFunction * bfunction : binterface->getMethods()) {
            values.push_back(btype->getMethod(bfunction->name)->function);
        }

        ArrayRef<Constant *> valuesRef(values);
        Constant * vTableData = ConstantStruct::get(binterface->vTableStructType, valuesRef);
        GlobalVariable * vTableDataVariable = new GlobalVariable(*currentPackage->currentModule->module, binterface->vTableStructType, true, GlobalValue::ExternalLinkage, vTableData, className + "_" + binterface->toString() + "_vtable");
        btype->interfaceVTables[x.first] = vTableDataVariable;
    }

    return std::any();
}