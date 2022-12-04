#include "BalanceInterface.h"

bool BalanceInterface::finalized() {
    for (auto const &x : this->getMethods()) {
        if (!x.second->finalized()) {
            return false;
        }
    }

    return true;
}