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