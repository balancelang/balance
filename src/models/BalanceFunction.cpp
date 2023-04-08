#include "BalanceFunction.h"

llvm::Function * BalanceFunction::getLlvmFunction(BalanceModule * bmodule) {
    if (this->balanceModule == bmodule) {
        return this->function;
    }

    for (auto const &x : this->imports) {
        if (x.first == bmodule) {
            return x.second;
        }
    }

    throw std::runtime_error("Failed to find implementation for function: " + this->name);
}

bool BalanceFunction::finalized() {
    return this->function != nullptr;
}

void BalanceFunction::setLlvmFunction(llvm::Function * function) {
    this->function = function;
}

llvm::Function * BalanceFunction::getFunctionDefinition() {
    return this->function;
}

llvm::FunctionType * BalanceFunction::getLlvmFunctionType() {
    std::vector<llvm::Type *> parameterTypes;
    for (BalanceParameter * bparameter : this->parameters) {
        parameterTypes.push_back(bparameter->balanceType->getReferencableType());
    }
    return llvm::FunctionType::get(this->returnType->getReferencableType(), parameterTypes, false);
}

std::string BalanceFunction::getFunctionName() {
    return this->name;
}

std::string BalanceFunction::getFullyQualifiedFunctionName() {
    if (this->balanceType != nullptr) {
        return this->balanceType->toString() + "_" + this->getFunctionName();
    } else {
        return this->getFunctionName();
    }
}
