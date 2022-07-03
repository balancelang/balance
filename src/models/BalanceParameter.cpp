#include "BalanceParameter.h"

bool BalanceParameter::finalized() {
    return this->type != nullptr;
}