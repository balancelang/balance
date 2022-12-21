#include "BalanceTypeString.h"
#include "../visitors/Visitor.h"
#include "../BalancePackage.h"

extern BalancePackage *currentPackage;

bool BalanceTypeString::isSimpleType() {
    if (this->base == "Int") {
        return true;
    } else if (this->base == "Bool") {
        return true;
    } else if (this->base == "Double") {
        return true;
    } else if (this->base == "None") {
        return true;
    }

    return false;
}

bool BalanceTypeString::isFloatingPointType() {
    // We might introduce other sized floating point types
    return this->base == "Double";
}

std::string BalanceTypeString::toString() {
    std::string result = this->base;
    auto genericsCount = this->generics.size();
    if (genericsCount > 0) {
        result += "<";
        for (int i = 0; i < genericsCount; i++) {
            result += this->generics[i]->toString();
            if (i < genericsCount - 1) {
                result += ", ";
            }
        }
        result += ">";
    }

    return result;
}

bool BalanceTypeString::finalized() {
    if (this->type == nullptr) {
        return false;
    }

    for (BalanceTypeString * generic : this->generics) {
        if (!generic->finalized()) {
            return false;
        }
    }

    return true;
}

void BalanceTypeString::populateTypes() {
    if (this->type == nullptr) {
        llvm::Type * t = getBuiltinType(this);
        if (t == nullptr) {
            BalanceType * btype = currentPackage->currentModule->getType(this);
            if (btype == nullptr) {
                throw std::runtime_error("Failed to populate type: " + this->toString());
            }
            this->type = btype->getReferencableType();
        } else {
            this->type = t;
        }
    }
}

bool BalanceTypeString::equalTo(BalanceTypeString * other) {
    // TODO: Maybe implement a better way to check for equality
    // 1. We at least need to check which module the type is from
    return this->toString() == other->toString();
}