#include "InheritanceVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceLambda.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

std::any InheritanceVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string className = ctx->className->getText();

    // For each known generic use of class, visit the class
    std::vector<BalanceType *> btypes = {};

    if (ctx->classGenerics()) {
        btypes = currentPackage->currentModule->getGenericVariants(className);
        btypes.push_back(currentPackage->currentModule->genericTypes[className]);
    } else {
        BalanceType * btype = currentPackage->currentModule->getType(className);
        btypes.push_back(btype);
    }

    for (BalanceType * btype : btypes) {
        currentPackage->currentModule->currentType = btype;
        visit(ctx->classExtendsImplements());
        currentPackage->currentModule->currentType = nullptr;
    }

    return nullptr;
}

std::any InheritanceVisitor::visitClassExtendsImplements(BalanceParser::ClassExtendsImplementsContext *ctx) {
    string text = ctx->getText();

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

    if (ctx->interfaces) {
        for (BalanceParser::BalanceTypeContext *type: ctx->interfaces->balanceType()) {
            std::string interfaceName = type->getText();
            BalanceType * btype = currentPackage->currentModule->getType(interfaceName);
            if (btype == nullptr) {
                currentPackage->currentModule->addTypeError(ctx, "Unknown interface: " + interfaceName);
                continue;
            }

            if (!btype->isInterface) {
                currentPackage->currentModule->addTypeError(ctx, "This is not an interface: " + interfaceName);
                continue;
            }
            currentPackage->currentModule->currentType->interfaces[interfaceName] = btype;
        }
    }

    return nullptr;
}