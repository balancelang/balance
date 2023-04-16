#include "Lambda.h"

#include "../Builtins.h"
#include "../BalancePackage.h"
#include "assert.h"

extern BalancePackage *currentPackage;

void LambdaBalanceType::registerType() {
    this->balanceType = new BalanceType(currentPackage->currentModule, "Lambda", {});

    // This registers the base type, which can't be instantiated directly.
    currentPackage->builtinModules["builtins"]->genericTypes["Lambda"] = this->balanceType;
}

void LambdaBalanceType::finalizeType() {

}
void LambdaBalanceType::registerMethods() {

}
void LambdaBalanceType::finalizeMethods() {

}
void LambdaBalanceType::registerFunctions() {

}
void LambdaBalanceType::finalizeFunctions() {

}

BalanceType * LambdaBalanceType::registerGenericType(std::vector<BalanceType *> generics) {
    BalanceType * lambdaType = new BalanceType(currentPackage->currentModule, "Lambda", generics);
    currentPackage->currentModule->addType(lambdaType);
    return lambdaType;
}

void LambdaBalanceType::tryFinalizeGenericType(BalanceType * balanceType) {
    assert(balanceType->name == "Lambda");

    std::vector<llvm::Type *> functionParameterTypes;
    //                                              -1 since last parameter is return
    for (int i = 0; i < balanceType->generics.size() - 1; i++) {
        if (!balanceType->generics[i]->finalized()) {
            return;
        }
        functionParameterTypes.push_back(balanceType->generics[i]->getReferencableType());
    }

    ArrayRef<Type *> parametersReference(functionParameterTypes);

    BalanceType * returnType = balanceType->generics.back();
    if (!returnType->finalized()) {
        return;
    }

    balanceType->internalType = FunctionType::get(returnType->getReferencableType(), parametersReference, false);
    balanceType->hasBody = true;
}