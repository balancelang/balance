#ifndef BALANCE_LAMBDA_H
#define BALANCE_LAMBDA_H

#include "BalanceParameter.h"
#include "BalanceModule.h"
#include "BalanceType.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"

class BalanceModule;
class BalanceParameter;
class BalanceType;

class BalanceLambda
{
public:
    BalanceType * returnType;
    std::vector<BalanceParameter *> parameters;
    bool hasExplicitReturn = false;

    BalanceLambda(std::vector<BalanceParameter *> parameters, BalanceType * returnType)
    {
        this->parameters = parameters;
        this->returnType = returnType;
    }
};

#endif