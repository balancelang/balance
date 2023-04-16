#ifndef PACKAGE_H
#define PACKAGE_H

#include "visitors/Visitor.h"
#include "Builtins.h"
#include "builtins/BuiltinType.h"

#include <string>
#include <map>
#include <filesystem>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "rapidjson/document.h"

class BalancePackage {
public:
    std::string packageJsonPath;
    std::string entrypoint;
    std::filesystem::path packageRootPath;
    rapidjson::Document document;
    std::filesystem::path buildDirectory = "build";

    std::string name;
    std::string version;
    std::map<std::string, std::string> entrypoints = {};
    std::map<std::string, BalanceModule *> modules = {};
    std::map<std::string, BalanceModule *> builtinModules = {};
    std::vector<BuiltinType *> nativeTypes = {};
    std::vector<BuiltinType *> builtinTypes = {};
    BalanceModule *currentModule = nullptr;
    llvm::LLVMContext *context;
    bool isAnalyzeOnly = false;
    bool verboseLogging = false;

    llvm::GlobalVariable * typeInfoTable = nullptr;
    llvm::StructType * typeInfoStructType = nullptr;

    std::function<void(std::string)> logger;

    BalancePackage(std::string packageJsonPath, std::string entrypoint, bool verboseLogging) {
        this->packageJsonPath = packageJsonPath;
        this->entrypoint = entrypoint;
        this->verboseLogging = verboseLogging;
        this->context = new llvm::LLVMContext();

        // Get directory of packageJson
        int pos = packageJsonPath.find_last_of("\\/");
        this->packageRootPath = packageJsonPath.substr(0, pos);

        this->logger = [&](std::string x) {
            if (this->verboseLogging) {
                std::cout << "LOG: " << x << std::endl;
            }
        };

        this->nativeTypes.push_back(new Int8PointerType());
        this->nativeTypes.push_back(new Int64PointerType());
        this->nativeTypes.push_back(new Int64Type());

        // this->builtinTypes = getBuiltinTypes();
        this->builtinTypes.push_back(new NoneBalanceType());
        this->builtinTypes.push_back(new IntType());
        this->builtinTypes.push_back(new AnyType());
        this->builtinTypes.push_back(new BoolType());
        this->builtinTypes.push_back(new DoubleType());
        this->builtinTypes.push_back(new StringType());
        this->builtinTypes.push_back(new TypeBalanceType());
        this->builtinTypes.push_back(new FileBalanceType());
        this->builtinTypes.push_back(new FatPointerType());
        this->builtinTypes.push_back(new ArrayBalanceType());
        this->builtinTypes.push_back(new LambdaBalanceType());
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

    bool execute(bool isScript = false);
    // bool executeAsScript();
    bool addBuiltinSource(std::string name, std::string code);
    bool executeString(std::string program);
    void load();
    void populate();
    void throwIfMissing(std::string property);
    bool compile(std::string name, std::vector<BalanceSource *> sources);
    // void buildLanguageServerTokens();
    void buildDependencyTree(BalanceModule * bmodule);
    BalanceModule * getNextElementOrNull();
    bool registerTypes(std::map<std::string, BalanceModule *> modules);
    bool registerGenericTypes(std::map<std::string, BalanceModule *> modules);
    bool registerInheritance(std::map<std::string, BalanceModule *> modules);
    bool buildTextualRepresentations(std::map<std::string, BalanceModule *> modules);
    bool finalizeProperties(std::map<std::string, BalanceModule *> modules);
    bool typeChecking(std::map<std::string, BalanceModule *> modules);
    void buildStructures(std::map<std::string, BalanceModule *> modules);
    void buildVTables(std::map<std::string, BalanceModule *> modules);
    void buildConstructors(std::map<std::string, BalanceModule *> modules);
    void buildForwardDeclarations(std::map<std::string, BalanceModule *> modules);
    void addBuiltinTypesToModules(std::map<std::string, BalanceModule *> modules);
    void addBuiltinFunctionsToModules(std::map<std::string, BalanceModule *> modules);
    bool compileBuiltins();
    bool compileModules(std::map<std::string, BalanceModule *> modules);
    void writePackageToBinary(std::string entrypointName);
    void llvmCompile(std::map<std::string, BalanceModule *> modules);
    void writeModuleToBinary(BalanceModule * bmodule);
    void registerAllTypes();
    void initializeTypeInfoTables(std::vector<BalanceType *> types);
    std::vector<BalanceType *> getAllTypes();
    BuiltinType * getBuiltinType(std::string typeName);
    void finalizeTypesAndFunctions();
    void registerTypesAndFunctions();
    void registerInitializers(std::map<std::string, BalanceModule *> modules);
    void finalizeInitializers(std::map<std::string, BalanceModule *> modules);
    void finalizeImports(std::map<std::string, BalanceModule *> modules);
};

#endif