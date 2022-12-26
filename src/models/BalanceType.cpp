#include "BalanceType.h"
#include "../BalancePackage.h"

extern BalancePackage * currentPackage;

void BalanceType::addMethod(std::string name, BalanceFunction *method) {
    this->methods[name] = method;
}

std::vector<BalanceFunction *> BalanceType::getMethods() {
    std::vector<BalanceFunction *> result = {};
    for (auto const &x : this->methods) {
        BalanceFunction * bfunction = x.second;
        result.push_back(bfunction);
    }

    for (BalanceType * parent : this->parents) {
        for (BalanceFunction * bfunction : parent->getMethods()) {
            result.push_back(bfunction);
        }
    }
    return result;
}

BalanceFunction * BalanceType::getMethod(std::string key) {
    for (BalanceFunction * bfunction : this->getMethods()) {
        if (bfunction->name == key) {
            return bfunction;
        }
    }
    return nullptr;
}

int BalanceType::getMethodIndex(std::string key) {
    auto it = this->methods.find(key);
    if (it == this->methods.end()) {
        return -1;
    }
    return std::distance(this->methods.begin(), it);
}

llvm::Type * BalanceType::getReferencableType() { return this->isSimpleType ? this->internalType : (llvm::Type *)this->internalType->getPointerTo(); }

llvm::Type * BalanceType::getInternalType() { return this->internalType; }

bool BalanceType::isFloatingPointType() {
    // We might introduce other sized floating point types
    return this->name == "Double";
}

std::string BalanceType::toString() {
    std::string result = this->name;
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

bool BalanceType::equalTo(BalanceModule *bmodule, std::string name, std::vector<BalanceType *> generics) {
    return this->equalTo(new BalanceType(bmodule, name, generics));
}

bool BalanceType::equalTo(BalanceType *other) {
    if (this->name != other->name) {
        return false;
    }

    if (this->generics.size() != other->generics.size()) {
        return false;
    }

    for (int i = 0; i < this->generics.size(); i++) {
        BalanceType *thisI = this->generics[i];
        BalanceType *otherI = other->generics[i];

        bool subResult = thisI->equalTo(otherI);
        if (!subResult) {
            return false;
        }
    }

    return true;
}

void BalanceType::addParent(BalanceType * parentType) {
    this->parents.push_back(parentType);
}

llvm::Function * BalanceType::getConstructor() {
    // TODO: When constructor overloading, change this signature to include e.g. parameters?
    return this->constructor;
}

BalanceProperty * BalanceType::getProperty(std::string propertyName) {
    for (BalanceProperty * bproperty : this->getProperties()) {
        if (bproperty->name == propertyName) {
            return bproperty;
        }
    }

    return nullptr;
}

bool BalanceType::finalized() {
    if (!this->hasBody) {
        return false;
    }

    for (auto const &x : this->methods) {
        if (x.second->function == nullptr) {
            return false;
        }
    }

    return true;
}

std::vector<BalanceProperty *> BalanceType::getProperties() {
    std::vector<BalanceProperty *> result = {};

    for (auto const &x : this->properties) {
        BalanceProperty *prop = x.second;
        result.push_back(prop);
    }

    for (BalanceType * parent : this->parents) {
        for (BalanceProperty * property : parent->getProperties()) {
            result.push_back(property);
        }
    }

    return result;
}

vector<BalanceType *> BalanceType::getHierarchy() {
    std::vector<BalanceType *> result = {};

    BalanceType * currentType = this;
    while (currentType != nullptr) {
        result.push_back(currentType);
        if (currentType->parents.size() > 0) {
            currentType = currentType->parents[0];
        } else {
            currentType = nullptr;
        }
    }

    return result;
}