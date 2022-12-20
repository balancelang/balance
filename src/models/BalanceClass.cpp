#include "BalanceClass.h"

bool BalanceClass::finalized() {
    if (!this->name->finalized()) {
        return false;
    }

    for (auto const &x : this->properties) {
        if (!x.second->finalized()) {
            return false;
        }
    }

    for (auto const &x : this->getMethods()) {
        if (!x.second->finalized()) {
            return false;
        }
    }

    if (!this->hasBody) {
        return false;
    }

    return true;
}