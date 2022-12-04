#include "ClassVTableVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::any ClassVTableVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    std::string text = ctx->getText();
    string className = ctx->className->getText();

    BalanceTypeString * btypeString = new BalanceTypeString(className);

    BalanceClass *bclass = currentPackage->currentModule->getClass(btypeString);

    // For each interface implemented by class, define this class' implementation of that interface's vtable type
    for (auto const &x : bclass->interfaces) {
        BalanceInterface * binterface = x.second;

        vector<Constant *> values;
        for (auto const &y : binterface->getMethods()) {
            values.push_back(bclass->getMethod(y.first)->function);
        }

        ArrayRef<Constant *> valuesRef(values);
        Constant * vTableData = ConstantStruct::get(binterface->vTableStructType, valuesRef);
        GlobalVariable * vTableDataVariable = new GlobalVariable(*currentPackage->currentModule->module, binterface->vTableStructType, true, GlobalValue::ExternalLinkage, vTableData, className + "_" + binterface->name->toString() + "_vtable");
        bclass->interfaceVTables[x.first] = vTableDataVariable;
    }

    return std::any();
}