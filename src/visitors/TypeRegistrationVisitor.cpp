#include "TypeRegistrationVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceType.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"
#include "Visitor.h"

#include "antlr4-runtime.h"
#include "config.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeindex>
#include <typeinfo>

using namespace antlrcpptest;
using namespace std;

extern BalancePackage *currentPackage;

std::any TypeRegistrationVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string className = ctx->className->getText();
    string text = ctx->getText();

    // Check for duplicate className
    BalanceType * btype = currentPackage->currentModule->getType(className);
    if (btype != nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Duplicate class, type already exist: " + btype->toString());
        throw TypeRegistrationVisitorException();
    }

    currentPackage->currentModule->addType(new BalanceType(currentPackage->currentModule, className, llvm::StructType::create(*currentPackage->context, className)));

    return nullptr;
}

std::any TypeRegistrationVisitor::visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) {
    string interfaceName = ctx->interfaceName->getText();
    string text = ctx->getText();

    // TODO: check for duplicate interfaceName
    BalanceType * btype = currentPackage->currentModule->getType(interfaceName);
    if (btype != nullptr) {
        currentPackage->currentModule->addTypeError(ctx, "Duplicate interface, type already exist: " + btype->toString());
        throw TypeRegistrationVisitorException();
    }

    BalanceType *binterface = new BalanceType(currentPackage->currentModule, interfaceName);
    binterface->isInterface = true;
    binterface->hasBody = true;

    BalanceType * fatPointerType = currentPackage->currentModule->getType("FatPointer");

    binterface->internalType = fatPointerType->getInternalType(); // ->getReferencableType(); //llvm::StructType::create(*currentPackage->context, interfaceName);
    currentPackage->currentModule->currentType = binterface;
    currentPackage->currentModule->addType(binterface);

    // Visit all class functions
    for (auto const &x : ctx->interfaceElement()) {
        if (x->functionSignature()) {
            visit(x);
        }
    }

    currentPackage->currentModule->currentType = nullptr;
    return nullptr;
}
