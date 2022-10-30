#ifndef BALANCE_LAMBDA_H
#define BALANCE_LAMBDA_H

#include "BalanceParameter.h"
#include "BalanceModule.h"
#include "BalanceTypeString.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"

class BalanceModule;


class BalanceLambda
{
public:
    BalanceTypeString * returnTypeString;
    std::vector<BalanceParameter *> parameters;
    bool hasExplicitReturn = false;

    BalanceLambda(std::vector<BalanceParameter *> parameters, BalanceTypeString * returnTypeString)
    {
        this->parameters = parameters;
        this->returnTypeString = returnTypeString;
    }
};


// TODO: Test importing a lambda - do we need "BalanceImportedLambda" ?
#endif