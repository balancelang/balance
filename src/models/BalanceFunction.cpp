#include "BalanceFunction.h"

#include "../Utilities.h"

BalanceImportedFunction::BalanceImportedFunction(BalanceModule * bmodule, BalanceFunction * bfunction) {
    this->bmodule = bmodule;
    this->bfunction = bfunction;
}

llvm::Function * BalanceImportedFunction::getLlvmFunction() {
    if (this->function == nullptr) {
        this->function = llvm::Function::Create(this->bfunction->getLlvmFunctionType(), llvm::Function::ExternalLinkage, this->bfunction->getFullyQualifiedFunctionName(), this->bmodule->module);
    }

    return this->function;
}

llvm::Function * BalanceFunction::getLlvmFunction(BalanceModule * bmodule) {
    if (this->balanceModule == bmodule) {
        return this->function;
    }

    for (auto const &x : this->imports) {
        if (x.first == bmodule) {
            return x.second->getLlvmFunction();
        }
    }

    // Otherwise lazily create the import
    if (bmodule->isTypeImported(this->balanceType)) {
        createImportedFunction(bmodule, this);

        for (auto const &x : this->imports) {
            if (x.first == bmodule) {
                return x.second->getLlvmFunction();
            }
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
    std::string functionName = this->name;

    for (BalanceParameter * bparameter : this->parameters) {
        functionName += "_" + bparameter->balanceType->toString();
    }

    functionName += "_" + this->returnType->toString();

    return functionName;
}

std::string BalanceFunction::getFullyQualifiedFunctionName() {
    if (this->balanceType != nullptr) {
        return this->balanceType->toString() + "_" + this->getFunctionName();
    } else {
        return this->getFunctionName();
    }
}
