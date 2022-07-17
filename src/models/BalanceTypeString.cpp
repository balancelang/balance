#include "BalanceTypeString.h"
#include "../visitors/Visitor.h"
#include "../Package.h"

extern BalancePackage *currentPackage;

std::string BalanceTypeString::toString() {
    std::string result = this->base;
    if (this->generics.size() > 0) {
        for (BalanceTypeString * generic : this->generics) {
            result += "<" + generic->toString() + ">";
        }
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
        // BalanceTypeString * thisTypeString = this;
        llvm::Type * t = getBuiltinType(this);
        if (t == nullptr) {
            BalanceClass * bclass = currentPackage->currentModule->getClass(this);
            if (bclass == nullptr) {
                BalanceImportedClass *ibclass = currentPackage->currentModule->getImportedClass(this);
                if (ibclass == nullptr) {
                    // TODO: Throw error
                } else {
                    this->type = ibclass->bclass->structType->getPointerTo();
                }
            } else {
                this->type = bclass->structType->getPointerTo();
            }
        } else {
            this->type = t;
        }
    }

    // for (BalanceTypeString * generic : this->generics) {
    //     generic->populateTypes();
    // }
}