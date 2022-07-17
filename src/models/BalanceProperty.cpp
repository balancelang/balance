#include "BalanceProperty.h"

bool BalanceProperty::finalized() {
    return this->type != nullptr;
}