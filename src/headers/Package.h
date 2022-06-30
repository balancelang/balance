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

class BalancePackage {
public:
    std::string packageJsonPath;
    std::string entrypoint;
    rapidjson::Document document;

    std::string name;
    std::string version;
    std::map<std::string, std::string> entrypoints = {};
    std::map<std::string, BalanceModule *> modules = {};
    BalanceModule *currentModule = nullptr;
    LLVMContext *context;

    BalancePackage(std::string packageJsonPath, std::string entrypoint) {
        this->packageJsonPath = packageJsonPath;
        this->entrypoint = entrypoint;
        this->context = new LLVMContext();
    }

    void reset() {
        this->modules = {};
        this->currentModule = nullptr;
    }

    bool execute();
    bool executeAsScript();
    bool executeString(std::string program);
    void load();
    void populate();
    void throwIfMissing(std::string property);
    bool compileAndPersist();
    void compile();
    void buildDependencyTree(std::string rootPath);
    void writePackageToBinary(std::string entrypointName);
    BalanceModule * getNextElementOrNull();
    void buildTextualRepresentations();
    void buildStructures();
    void buildForwardDeclarations();
    void buildConstructors();
};

#endif