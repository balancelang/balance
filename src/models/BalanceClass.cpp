#include "BalanceClass.h"

bool BalanceClass::finalized() {
    bool empty = this->properties.empty();
    for (auto const &x : this->properties) {
        if (!x.second->finalized()) {
            return false;
        }
    }

    for (auto const &x : this->methods) {
        if (!x.second->finalized()) {
            return false;
        }
    }

    if (!this->hasBody) {
        return false;
    }

    return true;
}

BalanceProperty * BalanceClass::getProperty(std::string propertyName) {
    if (this->properties.find(propertyName) != this->properties.end()) {
        return this->properties[propertyName];
    }
    return nullptr;
}