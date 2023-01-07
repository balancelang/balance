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
    llvm::LLVMContext *context;
    bool isAnalyzeOnly = false;
    bool verboseLogging = false;

    std::function<void(std::string)> logger;

    BalancePackage(std::string packageJsonPath, std::string entrypoint, bool verboseLogging) {
        this->packageJsonPath = packageJsonPath;
        this->entrypoint = entrypoint;
        this->verboseLogging = verboseLogging;
        this->context = new llvm::LLVMContext();

        // Get directory of packageJson
        int pos = packageJsonPath.find_last_of("\\/");
        this->packagePath = packageJsonPath.substr(0, pos);

        this->logger = [&](std::string x) {
            if (this->verboseLogging) {
                std::cout << "LOG: " << x << std::endl;
            }
        };
    }

    void reset() {
        this->modules = {};
        this->currentModule = nullptr;
    }

    BalanceModule * getModule(std::string modulePath) {
        if (this->modules.find(modulePath) != this->modules.end()) {
            return this->modules[modulePath];
        }
        return nullptr;
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
    bool buildTextualRepresentations();
    void runLLVMFunctionVisitor();
    bool registerTypes();
    bool registerGenericTypes();
    bool registerInheritance();
    bool finalizeProperties();
    void buildStructures();
    void buildForwardDeclarations();
    void buildVTables();
    void buildConstructors();
    void addBuiltinsToModules();
    bool typeChecking();
};

#endif