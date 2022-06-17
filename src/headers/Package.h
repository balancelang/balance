#ifndef PACKAGE_H
#define PACKAGE_H

#include "Visitor.h"
#include "Builtins.h"

#include <string>
#include <map>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "rapidjson/document.h"

class Package {
public:
    LLVMContext *context;
    Module *module;
    IRBuilder<> *builder;
    vector<BalanceType> types;
    ScopeBlock *currentScope;
    bool verbose = false;

    std::string packageJsonPath;
    std::string entrypoint;
    rapidjson::Document document;

    std::string name;
    std::string version;
    std::map<std::string, std::string> entrypoints = {};
    std::map<std::string, llvm::Module *> modules = {};

    Package(std::string packageJsonPath, std::string entrypoint) {
        this->packageJsonPath = packageJsonPath;
        this->entrypoint = entrypoint;
    }

    bool execute();
    void load();
    void populate();
    void throwIfMissing(std::string property);
    bool compileAndPersist();
    void compileRecursively(std::string path, std::vector<Module *> * modules);
};

#endif