#include "BalanceType.h"
#include "../BalancePackage.h"
#include "../Utilities.h"

extern BalancePackage * currentPackage;

void BalanceType::addMethod(BalanceFunction *method) {
    this->methods.push_back(method);
}

void BalanceType::addConstructor(std::string name, std::vector<BalanceParameter *> parameters) {
    std::string constructorName = name + std::to_string(this->constructors.size());
    BalanceFunction * constructor = new BalanceFunction(currentPackage->currentModule, this, constructorName, parameters, currentPackage->currentModule->getType("None"));
    this->constructors.push_back(constructor);
}

std::vector<BalanceFunction *> BalanceType::getMethods() {
    std::vector<BalanceFunction *> result = {};
    for (BalanceFunction * bfunction : this->methods) {
        result.push_back(bfunction);
    }

    for (BalanceType * parent : this->parents) {
        for (BalanceFunction * bfunction : parent->getMethods()) {
            result.push_back(bfunction);
        }
    }
    return result;
}

BalanceFunction * BalanceType::getMethod(std::string functionName, std::vector<BalanceType *> parameters) {
    for (BalanceFunction * bfunction : this->getMethods()) {
        if (functionEqualTo(bfunction, functionName, parameters)) {
            return bfunction;
        }
    }
    return nullptr;
}

std::vector<BalanceFunction *> BalanceType::getMethodsByName(std::string methodName) {
    std::vector<BalanceFunction *> methods;

    for (BalanceFunction * bfunction : getMethods()) {
        if (bfunction->name == methodName) {
            methods.push_back(bfunction);
        }
    }

    return methods;
}

int BalanceType::getMethodIndex(std::string methodName, std::vector<BalanceType *> types) {
    for (int i = 0; i < this->methods.size(); i++) {
        if (functionEqualTo(this->methods[i], methodName, types)) {
            return i;
        }
    }

    return -1;
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

bool BalanceType::equalTo(std::string name, std::vector<BalanceType *> generics) {
    if (this->name != name) {
        return false;
    }

    if (this->generics.size() != generics.size()) {
        return false;
    }

    for (int i = 0; i < this->generics.size(); i++) {
        BalanceType *thisI = this->generics[i];
        BalanceType *otherI = generics[i];

        bool subResult = thisI->equalTo(otherI);
        if (!subResult) {
            return false;
        }
    }

    return true;
}

bool BalanceType::equalTo(BalanceType *other) {
    return this->equalTo(other->name, other->generics);
}

void BalanceType::addParent(BalanceType * parentType) {
    this->parents.push_back(parentType);
}

BalanceFunction * BalanceType::getInitializer() {
    return this->initializer;
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
    if (!this->isSimpleType && !this->hasBody) {
        return false;
    }

    for (BalanceFunction * bfunction : this->methods) {
        if (!bfunction->finalized()) {
            return false;
        }
    }

    return true;
}

std::vector<BalanceProperty *> BalanceType::getProperties() {
    std::vector<BalanceProperty *> result = {};

    vector<BalanceType *> hierarchy = this->getHierarchy();
    std::reverse(hierarchy.begin(), hierarchy.end());

    for (BalanceType * member : hierarchy) {
        for (auto const &x : member->properties) {
            BalanceProperty * property = x.second;
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

BalanceFunction * BalanceType::getConstructor(std::vector<BalanceType *> parameters) {
    for (BalanceFunction * constructor : this->constructors) {
        if (constructor->parameters.size() != parameters.size()) {
            continue;
        }
        bool hasError = false;
        for (int i = 0; i < constructor->parameters.size(); i++) {
            if (!constructor->parameters[i]->balanceType->equalTo(parameters[i])) {
                hasError = true;
                break;
            }
        }

        if (!hasError) {
            return constructor;
        }
    }

    return nullptr;
}