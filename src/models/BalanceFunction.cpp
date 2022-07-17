#include "BalanceFunction.h"

bool BalanceFunction::hasAllTypes() {
    for (auto const &x : this->parameters) {
        if (!x->finalized()) {
            return false;
        }
    }

    if (returnType == nullptr) {
        return false;
    }

    return true;
}

bool BalanceFunction::finalized() {
    if (!this->hasAllTypes()) {
        return false;
    }

    if (function == nullptr) {
        return false;
    }

    return true;
}