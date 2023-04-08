#include "Lambda.h"

#include "../Builtins.h"
#include "../BalancePackage.h"

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

    vector<Type *> functionParameterTypes;
    //                                              -1 since last parameter is return
    for (int i = 0; i < generics.size() - 1; i++) {
        functionParameterTypes.push_back(generics[i]->getReferencableType());
    }

    ArrayRef<Type *> parametersReference(functionParameterTypes);

    Type * returnType = generics.back()->getReferencableType();
    lambdaType->internalType = FunctionType::get(returnType, parametersReference, false);
    lambdaType->hasBody = true;

    createDefaultConstructor(currentPackage->currentModule, lambdaType);

    currentPackage->currentModule->currentType = nullptr;

    return lambdaType;
}
