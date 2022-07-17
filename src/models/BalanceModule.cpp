#include "BalanceModule.h"
#include "../Package.h"

extern BalancePackage *currentPackage;

void BalanceModule::initializeModule() {
    this->builder = new llvm::IRBuilder<>(*currentPackage->context);
    this->module = new llvm::Module(this->path, *currentPackage->context);

    // Initialize module root scope
    FunctionType *funcType = FunctionType::get(this->builder->getInt32Ty(), false);
    std::string rootFunctionName = this->path + "_main";
    Function *rootFunc = Function::Create(funcType, Function::ExternalLinkage, this->isEntrypoint ? "main" : rootFunctionName, this->module);
    BasicBlock *entry = BasicBlock::Create(*currentPackage->context, "entrypoint", rootFunc);
    this->builder->SetInsertPoint(entry);
    this->rootScope = new BalanceScopeBlock(entry, nullptr);
    this->currentScope = this->rootScope;
}

void BalanceModule::generateASTFromStream(antlr4::ANTLRInputStream *stream) {
    this->antlrStream = stream;
    this->lexer = new BalanceLexer(this->antlrStream);
    this->tokenStream = new antlr4::CommonTokenStream(this->lexer);
    this->tokenStream->fill();
    this->parser = new BalanceParser(this->tokenStream);
    this->tree = this->parser->root();
}

void BalanceModule::generateASTFromPath(std::string filePath) {
    if (!fileExist(filePath)) {
        cout << "Input file doesn't exist: " << filePath << endl;
        exit(1);
    }

    ifstream inputStream;
    inputStream.open(filePath);
    this->antlrStream = new antlr4::ANTLRInputStream(inputStream);
    this->generateASTFromStream(antlrStream);
}

void BalanceModule::generateASTFromString(std::string program) {
    this->antlrStream = new antlr4::ANTLRInputStream(program);
    this->generateASTFromStream(antlrStream);
}

BalanceClass *BalanceModule::getClass(std::string className) {
    if (this->classes.find(className) != this->classes.end()) {
        return this->classes[className];
    }
    return nullptr;
}

BalanceFunction *BalanceModule::getFunction(std::string functionName) {
    if (this->functions.find(functionName) != this->functions.end()) {
        return this->functions[functionName];
    }
    return nullptr;
}

BalanceImportedClass *BalanceModule::getImportedClass(std::string className) {
    if (this->importedClasses.find(className) != this->importedClasses.end()) {
        return this->importedClasses[className];
    }
    return nullptr;
}

BalanceImportedFunction *BalanceModule::getImportedFunction(std::string functionName) {
    if (this->importedFunctions.find(functionName) != this->importedFunctions.end()) {
        return this->importedFunctions[functionName];
    }
    return nullptr;
}

llvm::Value *BalanceModule::getValue(std::string variableName) {
    BalanceScopeBlock *scope = this->currentScope;
    while (scope != nullptr) {
        // Check variables etc
        llvm::Value *tryVal = scope->symbolTable[variableName];
        if (tryVal != nullptr) {
            return tryVal;
        }

        scope = scope->parent;
    }

    return nullptr;
}

void BalanceModule::setValue(std::string variableName, llvm::Value *value) { this->currentScope->symbolTable[variableName] = value; }

bool BalanceModule::finalized() {
    for (auto const &x : this->classes) {
        if (!x.second->finalized()) {
            return false;
        }
    }

    for (auto const &x : this->functions) {
        if (!x.second->finalized()) {
            return false;
        }
    }

    return true;
}
