#ifndef BALANCE_FUNCTION_H
#define BALANCE_FUNCTION_H

#include "BalanceParameter.h"
#include "BalanceModule.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"

class BalanceModule;


class BalanceFunction
{
public:
    std::string name;
    std::string returnTypeString;
    std::vector<BalanceParameter *> parameters;
    llvm::Type *returnType = nullptr;
    llvm::Function *function = nullptr;
    // BalanceModule *module;

    BalanceFunction(std::string name, std::vector<BalanceParameter *> parameters, std::string returnTypeString)
    {
        this->name = name;
        this->parameters = parameters;
        this->returnTypeString = returnTypeString;
    }

    bool hasAllTypes();
    bool finalized();
};

class BalanceImportedFunction {
public:
    BalanceModule * module;                 // The module importing the function
    BalanceFunction * bfunction;            // The source function
    llvm::Function * function = nullptr;    // The re-declared function in the module

    BalanceImportedFunction(BalanceModule * module, BalanceFunction * bfunction) {
        this->module = module;
        this->bfunction = bfunction;
    }
};

#endif