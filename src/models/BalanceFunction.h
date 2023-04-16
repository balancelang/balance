#ifndef BALANCE_FUNCTION_H
#define BALANCE_FUNCTION_H

#include "BalanceParameter.h"
#include "BalanceModule.h"
#include "BalanceType.h"
#include "llvm/IR/Function.h"

class BalanceModule;
class BalanceParameter;
class BalanceType;
class BalanceFunction;


class BalanceImportedFunction
{
public:
    BalanceModule * bmodule = nullptr;
    BalanceFunction * bfunction = nullptr;
    llvm::Function * function = nullptr;

    BalanceImportedFunction(BalanceModule * bmodule, BalanceFunction * bfunction);
    llvm::Function * getLlvmFunction();
};


class BalanceFunction
{
public:
    std::string name;
    BalanceModule * balanceModule;
    BalanceType * balanceType;
    BalanceType * returnType;
    std::vector<BalanceParameter *> parameters;
    bool hasExplicitReturn = false;

    llvm::Function *function = nullptr;
    // Mapping from module to the forward declaration of that function
    std::map<BalanceModule *, BalanceImportedFunction *> imports = {};

    BalanceFunction(BalanceModule * balanceModule, BalanceType * balanceType, std::string name, std::vector<BalanceParameter *> parameters, BalanceType * returnType)
    {
        this->balanceModule = balanceModule;
        this->balanceType = balanceType;
        this->name = name;
        this->parameters = parameters;
        this->returnType = returnType;
    }

    bool finalized();
    llvm::Function * getLlvmFunction(BalanceModule * bmodule);
    void setLlvmFunction(llvm::Function * function);
    llvm::Function * getFunctionDefinition();

    llvm::FunctionType * getLlvmFunctionType();
    std::string getFunctionName();
    std::string getFullyQualifiedFunctionName();
};

#endif