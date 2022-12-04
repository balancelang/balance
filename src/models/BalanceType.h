#ifndef BALANCE_TYPE_H
#define BALANCE_TYPE_H

#include <map>

#include "BalanceTypeString.h"

class BalanceFunction;

class BalanceType
{
private:
    std::map<std::string, BalanceFunction *> methods = {};

public:
    BalanceTypeString * name;

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
};

#endif