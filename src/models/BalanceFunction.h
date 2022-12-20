#ifndef BALANCE_FUNCTION_H
#define BALANCE_FUNCTION_H

#include "BalanceParameter.h"
#include "BalanceModule.h"
#include "BalanceTypeString.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"

class BalanceModule;


class BalanceFunction
{
public:
    std::string name;
    BalanceTypeString * returnTypeString;
    std::vector<BalanceParameter *> parameters;
    llvm::Type *returnType = nullptr;
    llvm::Function *function = nullptr;
    bool hasExplicitReturn = false;

    BalanceFunction(std::string name, std::vector<BalanceParameter *> parameters, BalanceTypeString * returnTypeString)
    {
        this->name = name;
        this->parameters = parameters;
        this->returnTypeString = returnTypeString;
    }

    bool hasAllTypes();
    bool finalized();
};

#endif