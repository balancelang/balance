#include "FinalizePropertiesVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::any FinalizePropertiesVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
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
        currentPackage->currentModule->currentType = btype;

        // Iterate parents and lay out properties starting with the most deep ancestor
        vector<BalanceType *> hierarchy = btype->getHierarchy();
        std::reverse(hierarchy.begin(), hierarchy.end());

        int index = 0;
        for (BalanceType * member : hierarchy) {
            for (auto const &x : member->properties) {
                BalanceProperty * bproperty = x.second;
                bproperty->index = index;
                index++;
            }
        }
    }

    return nullptr;
}
