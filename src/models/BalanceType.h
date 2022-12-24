#ifndef BALANCE_TYPE_H
#define BALANCE_TYPE_H

#include <map>
#include "llvm/IR/Function.h"

#include "BalanceProperty.h"
#include "BalanceFunction.h"
#include "BalanceModule.h"

class BalanceFunction;
class BalanceProperty;
class BalanceModule;

class BalanceType
{
public:
    std::string name;                                   // E.g. "Array" in "Array<Int>"
    std::vector<BalanceType *> generics;                // E.g. ["String", "Int"] in Dictionary<String, Int>
    llvm::Type * internalType = nullptr;
    llvm::Function * constructor = nullptr;
    bool isSimpleType = false;
    bool isInterface = false;
    BalanceModule *balanceModule;
    bool hasBody = false;

    std::map<std::string, BalanceProperty *> properties = {};
    std::map<std::string, BalanceFunction *> methods = {};
    std::map<std::string, BalanceType *> interfaces = {};

    // If interface, this holds the vtable struct (the vtable function types)
    llvm::StructType *vTableStructType = nullptr;

    // If class, this holds each implemented vtable
    std::map<std::string, llvm::Value *> interfaceVTables = {};

    BalanceType(BalanceModule * balanceModule, std::string name, std::vector<BalanceType *> generics = {}) {
        this->balanceModule = balanceModule;
        this->name = name;
        this->generics = generics;
    }

    BalanceType(BalanceModule * balanceModule, std::string name, llvm::Type * internalType) {
        this->balanceModule = balanceModule;
        this->name = name;
        this->internalType = internalType;
    }

    void addMethod(std::string name, BalanceFunction * method);
    std::map<std::string, BalanceFunction *> getMethods();
    BalanceFunction * getMethod(std::string key);
    int getMethodIndex(std::string key);
    llvm::Type * getReferencableType();
    llvm::Type * getInternalType();
    bool isFloatingPointType();
    std::string toString();
    bool equalTo(BalanceModule * bmodule, std::string name, std::vector<BalanceType *> generics = {});
    bool equalTo(BalanceType * other);
    llvm::Function * getConstructor();
    BalanceProperty * getProperty(std::string propertyName);
    bool finalized();
};

#endif