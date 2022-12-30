#include "Lambda.h"

#include "../Builtins.h"
#include "../BalancePackage.h"

extern BalancePackage *currentPackage;

class BalanceType;

BalanceType * createType__Lambda(std::vector<BalanceType *> generics) {
    BalanceType * bclass = new BalanceType(currentPackage->currentModule, "Lambda", generics);
    currentPackage->currentModule->addType(bclass);

    currentPackage->currentModule->currentType = bclass;

    vector<Type *> functionParameterTypes;
    //                                              -1 since last parameter is return
    for (int i = 0; i < generics.size() - 1; i++) {
        functionParameterTypes.push_back(generics[i]->getReferencableType());
    }

    ArrayRef<Type *> parametersReference(functionParameterTypes);

    Type * returnType = generics.back()->getReferencableType();
    bclass->internalType = FunctionType::get(returnType, parametersReference, false);
    bclass->hasBody = true;

    createDefaultConstructor(currentPackage->currentModule, bclass);

    currentPackage->currentModule->currentType = nullptr;

    return bclass;
}