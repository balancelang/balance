#ifndef BALANCE_FUNCTION_H
#define BALANCE_FUNCTION_H

#include "BalanceParameter.h"
#include "BalanceModule.h"
#include "BalanceType.h"
#include "llvm/IR/Function.h"

class BalanceModule;
class BalanceParameter;
class BalanceType;

class BalanceFunction
{
public:
    std::string name;
    BalanceType * returnType;
    std::vector<BalanceParameter *> parameters;
    llvm::Function *function = nullptr;
    bool hasExplicitReturn = false;

    BalanceFunction(std::string name, std::vector<BalanceParameter *> parameters, BalanceType * returnType)
    {
        this->name = name;
        this->parameters = parameters;
        this->returnType = returnType;
    }
};

#endif