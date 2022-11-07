#ifndef PACKAGE_H
#define PACKAGE_H

#include "visitors/Visitor.h"
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
    std::string packagePath;
    rapidjson::Document document;

    std::string name;
    std::string version;
    std::map<std::string, std::string> entrypoints = {};
    std::map<std::string, BalanceModule *> modules = {};
    BalanceModule * builtins = nullptr;
    BalanceModule *currentModule = nullptr;
    LLVMContext *context;
    bool isAnalyzeOnly = false;

    std::function<void(std::string)> logger;

    BalancePackage(std::string packageJsonPath, std::string entrypoint) {
        this->packageJsonPath = packageJsonPath;
        this->entrypoint = entrypoint;
        this->context = new LLVMContext();

        // Get directory of packageJson
        int pos = packageJsonPath.find_last_of("\\/");
        this->packagePath = packageJsonPath.substr(0, pos);

        this->logger = [](std::string x) {
            std::cout << "LOG: " << x << std::endl;
        };
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
    void buildLanguageServerTokens();
    void buildDependencyTree(std::string rootPath);
    void writePackageToBinary(std::string entrypointName);
    BalanceModule * getNextElementOrNull();
    void buildTextualRepresentations();
    void buildStructures();
    void buildForwardDeclarations();
    void buildConstructors();
    void addBuiltinsToModules();
    bool typeChecking();
};

#endif