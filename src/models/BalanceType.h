#ifndef BALANCE_TYPE_H
#define BALANCE_TYPE_H

#include <map>
#include "llvm/IR/Function.h"

#include "BalanceTypeString.h"

class BalanceFunction;
class BalanceProperty;

class BalanceType
{
public:
    BalanceTypeString * name;
    llvm::Type * internalType = nullptr;
    llvm::Function * constructor = nullptr;
    bool isSimpleType = false;

    std::map<std::string, BalanceProperty *> properties = {};
    std::map<std::string, BalanceFunction *> methods = {};

    void addMethod(std::string name, BalanceFunction * method) {
        this->methods[name] = method;
    }

    std::map<std::string, BalanceFunction *> getMethods() {
        return this->methods;
    }

    BalanceFunction * getMethod(std::string key) {
        if (this->methods.find(key) != this->methods.end()) {
            return this->methods[key];
        }
        return nullptr;
    }

    int getMethodIndex(std::string key) {
        auto it = this->methods.find(key);
        if (it == this->methods.end()) {
            return -1;
        }
        return std::distance(this->methods.begin(), it);
    }

    llvm::Type * getReferencableType() {
        return this->isSimpleType ? this->internalType : (llvm::Type *) this->internalType->getPointerTo();
    }

    llvm::Type * getInternalType() {
        return this->internalType;
    }

    bool isInterface() {
        return this->name->isInterface;
    }

    llvm::Function * getConstructor() {
        // TODO: When constructor overloading, change this signature to include e.g. parameters?
        return this->constructor;
    }

    BalanceProperty * getProperty(std::string propertyName) {
        if (this->properties.find(propertyName) != this->properties.end()) {
            return this->properties[propertyName];
        }
        return nullptr;
    }
};

#endif