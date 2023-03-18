#include "BalancePackage.h"
#include "Main.h"
#include "Utilities.h"
#include "visitors/PackageVisitor.h"
#include "visitors/StructureVisitor.h"
#include "visitors/ForwardDeclarationVisitor.h"
#include "visitors/ConstructorVisitor.h"
#include "visitors/LLVMTypeVisitor.h"
#include "visitors/TypeVisitor.h"
#include "visitors/TokenVisitor.h"
#include "visitors/InterfaceVTableVisitor.h"
#include "visitors/ClassVTableVisitor.h"
#include "visitors/TypeRegistrationVisitor.h"
#include "visitors/GenericTypeRegistrationVisitor.h"
#include "visitors/InheritanceVisitor.h"
#include "visitors/FinalizePropertiesVisitor.h"
#include "standard-library/Range.h"
#include "builtins/Any.h"

#include "config.h"

#include <map>
#include <queue>
#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"

namespace fs = std::filesystem;

extern BalancePackage *currentPackage;

void BalancePackage::load()
{
    FILE *fp = fopen(this->packageJsonPath.c_str(), "r");

    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    this->document.ParseStream(is);

    fclose(fp);
}

void BalancePackage::throwIfMissing(std::string property)
{
    if (!this->document.HasMember(property.c_str()))
    {
        throw std::runtime_error("Missing property: " + property);
    }
}

void BalancePackage::populate()
{
    // Name
    throwIfMissing("name");
    this->name = this->document["name"].GetString();

    // Version
    throwIfMissing("version");
    this->version = this->document["version"].GetString();

    // Entrypoints
    throwIfMissing("entrypoints");
    const rapidjson::Value &entrypoints = document["entrypoints"];
    for (rapidjson::Value::ConstMemberIterator itr = entrypoints.MemberBegin(); itr != entrypoints.MemberEnd(); ++itr)
    {
        std::string entrypointName = itr->name.GetString();
        std::string entrypointValue = itr->value.GetString();
        std::string entrypointPath = this->packageRootPath / entrypointValue;
        this->entrypoints[entrypointName] = entrypointPath;

        // Check if entrypointValue exists?
        if (!fileExist(entrypointPath)) {
            std::cout << "Entrypoint does not exist: " << entrypointPath << std::endl;
            exit(1);
        }
    }

    // Dependencies
}

bool BalancePackage::execute(bool isScript)
{
    if (isScript) {
        // Already has entryPoint then
        this->name = "default";
    } else if (fileExist(this->packageJsonPath)) {
        // Load json file into properties
        this->load();
        this->populate();
    } else {
        // Find all balance scripts and add as entrypoints
        for (const auto & entry : fs::directory_iterator(this->packageRootPath)) {
            auto extension = entry.path().extension().string();
            if (extension == ".bl") {
                std::string filename = entry.path().filename().string();
                std::string filenameWithoutExtension = filename.substr(0, filename.find_last_of("."));
                this->entrypoints[filenameWithoutExtension] = entry.path();
            }
        }
    }

    std::filesystem::path p(this->entrypoint);
    auto source = new BalanceSource();
    source->filePath = std::filesystem::canonical(p);

    return this->compile(p.stem(), {source});
}

bool BalancePackage::compile(std::string name, std::vector<BalanceSource *> sources) {
    this->reset();

    BalanceModule * bmodule = new BalanceModule(name, sources, true);
    this->buildDependencyTree(bmodule);

    // Registers all builtin and user types
    this->registerAllTypes();

    this->compileBuiltins();

    bool success = this->compileModules(this->modules);
    if (success) {
        this->writePackageToBinary(this->name);
    }

    return success;
}

void BalancePackage::registerAllTypes() {
    // Register all builtin types
    createBuiltinTypes();
    this->addBuiltinSource("standard-library/range", getStandardLibraryRangeCode());
    this->registerTypes(this->builtinModules);
    this->registerGenericTypes(this->builtinModules);
    this->addBuiltinsToModules(this->builtinModules);
    this->addBuiltinsToModules(this->modules);

    // Register all user types
    this->registerTypes(this->modules);
    this->registerGenericTypes(this->modules);

    // Create typeInfoTable
    this->initializeTypeInfoTables(this->builtinModules);
    this->initializeTypeInfoTables(this->modules);
}

void BalancePackage::initializeTypeInfoTables(std::map<std::string, BalanceModule *> modules) {
    for (auto const &x : modules) {
        BalanceModule *bmodule = x.second;
        std::vector<Constant *> typeInfoVariables = {};
        for (BalanceType * btype : bmodule->types) {
            typeInfoVariables.push_back(btype->typeInfoVariable);
        }

        bmodule->initializeTypeInfoTable();
        ArrayRef<Constant *> valuesRef(typeInfoVariables);
        llvm::ArrayType * arrayType = llvm::ArrayType::get(bmodule->typeInfoStructType, typeInfoVariables.size());
        Constant * typeTableData = ConstantArray::get(arrayType, valuesRef);
        bmodule->typeInfoTable->setInitializer(typeTableData);
    }
}

bool BalancePackage::compileBuiltins() {
    this->currentModule = currentPackage->builtinModules["builtins"];
    createBuiltinFunctions();
    bool success = this->compileModules(this->builtinModules);
    return success;
}

bool BalancePackage::addBuiltinSource(std::string name, std::string code) {
    auto source = new BalanceSource();
    source->programString = code;
    BalanceModule * bmodule = new BalanceModule(name, {source}, false);
    bmodule->generateAST();
    currentPackage->builtinModules[bmodule->name] = bmodule;
    return true;
}

bool BalancePackage::compileModules(std::map<std::string, BalanceModule *> modules) {
    bool success = true;

    success = this->registerInheritance(modules);
    if (!success) return false;

    success = this->buildTextualRepresentations(modules);
    if (!success) return false;

    success = this->finalizeProperties(modules);
    if (!success) return false;

    success = this->typeChecking(modules);
    if (!success) return false;

    this->buildStructures(modules);
    this->buildVTables(modules);
    this->buildConstructors(modules);
    this->buildForwardDeclarations(modules);
    this->llvmCompile(modules);
    return true;
}

bool BalancePackage::executeString(std::string program) {
    currentPackage = this;
    this->name = "program";
    auto source = new BalanceSource();
    source->programString = program;
    return this->compile("program", {source});
}

void BalancePackage::buildConstructors(std::map<std::string, BalanceModule *> modules) {
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        if (bmodule->tree == nullptr) {
            continue;
        }

        this->currentModule = bmodule;

        // Visit entire tree
        ConstructorVisitor visitor;
        visitor.visit(bmodule->tree);
    }
}

void BalancePackage::buildStructures(std::map<std::string, BalanceModule *> modules)
{
    std::queue<BalanceModule *> queue;

    for (auto const &x : modules)
    {
        queue.push(x.second);
    }

    int iterations = 0;
    while (queue.size() > 0)
    {
        BalanceModule *bmodule = queue.front();
        this->currentModule = bmodule;
        queue.pop();

        if (bmodule->tree == nullptr) {
            continue;
        }

        // Visit entire tree
        LLVMTypeVisitor visitor;
        visitor.visit(bmodule->tree);

        bool isFinalized = true;

        // Check all types
        for (BalanceType * btype : bmodule->types) {
            if (!btype->finalized()) {
                isFinalized = false;
                break;
            }
        }

        // Check all functions
        for (BalanceFunction * bfunction : bmodule->functions) {
            if (bfunction->function == nullptr) {
                isFinalized = false;
                break;
            }
        }

        if (!isFinalized)
        {
            queue.push(bmodule);
        }

        // TODO: Check that we don't end up in a endless loop
        iterations++;

        if (iterations > 100)
        {
            std::cout << "Killed type visitor loop as it exceeded max iterations. Please file this as a bug." << std::endl;
            exit(1);
        }
    }
}

void BalancePackage::buildVTables(std::map<std::string, BalanceModule *> modules) {
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        if (bmodule->tree == nullptr) {
            continue;
        }

        this->currentModule = bmodule;

        // Visit entire tree
        InterfaceVTableVisitor visitor;
        visitor.visit(bmodule->tree);
    }

    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        if (bmodule->tree == nullptr) {
            continue;
        }

        this->currentModule = bmodule;

        // Visit entire tree
        ClassVTableVisitor visitor;
        visitor.visit(bmodule->tree);
    }
}

void BalancePackage::buildForwardDeclarations(std::map<std::string, BalanceModule *> modules)
{
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        if (bmodule->tree == nullptr) {
            continue;
        }

        this->currentModule = bmodule;

        // Visit entire tree
        ForwardDeclarationVisitor visitor;
        visitor.visit(bmodule->tree);
    }
}


void BalancePackage::addBuiltinsToModules(std::map<std::string, BalanceModule *> modules) {
    for (auto const &x : builtinModules) {
        BalanceModule * builtinsModule = x.second;

        for (auto const &x : modules)
        {
            BalanceModule *bmodule = x.second;
            if (bmodule->tree == nullptr) {
                continue;
            }

            this->currentModule = bmodule;

            for (BalanceFunction * bfunction : builtinsModule->functions) {
                createImportedFunction(bmodule, bfunction);
            }

            for (BalanceType * bclass : builtinsModule->types) {
                createImportedClass(bmodule, bclass);
            }

            for (auto const &x : builtinsModule->genericTypes) {
                // TODO: Assume we can just import them when we know the generic types
                bmodule->genericTypes[x.first] = x.second;
            }
        }
    }
}

bool BalancePackage::buildTextualRepresentations(std::map<std::string, BalanceModule *> modules)
{
    for (auto const &x : modules) {
        BalanceModule *bmodule = x.second;
        if (bmodule->tree == nullptr) {
            continue;
        }

        try {
            this->currentModule = bmodule;

            // Visit entire tree
            StructureVisitor visitor;
            visitor.visit(this->currentModule->tree);
        } catch (const StructureVisitorException& myException) {
            bmodule->reportTypeErrors();
            return false;
        }
    }

    return true;
}

bool BalancePackage::finalizeProperties(std::map<std::string, BalanceModule *> modules) {
    for (auto const &x : modules) {
        BalanceModule *bmodule = x.second;
        if (bmodule->tree == nullptr) {
            continue;
        }

        try {
            this->currentModule = bmodule;

            // Visit entire tree
            FinalizePropertiesVisitor visitor;
            visitor.visit(this->currentModule->tree);
        } catch (const FinalizePropertiesVisitor& myException) {
            bmodule->reportTypeErrors();
            return false;
        }
    }

    return true;
}

bool BalancePackage::registerInheritance(std::map<std::string, BalanceModule *> modules)
{
    BalanceType * anyType = currentPackage->builtinModules["builtins"]->getType("Any");

    for (auto const &x : currentPackage->builtinModules) {
        BalanceModule *bmodule = x.second;
        for (BalanceType * btype : bmodule->types) {
            if (btype->parents.size() == 0 && btype->name != "Any") {
                btype->addParent(anyType);
            }
        }
    }

    for (auto const &x : modules) {
        BalanceModule *bmodule = x.second;
        if (bmodule->tree == nullptr) {
            continue;
        }

        BalanceType * anyType = bmodule->getType("Any");
        try {
            this->currentModule = bmodule;

            // Visit entire tree
            InheritanceVisitor visitor;
            visitor.visit(this->currentModule->tree);

            // Assign any as base-class for all types without parents
            for (BalanceType * btype : bmodule->types) {
                if (btype->parents.size() == 0 && btype->name != "Any") {
                    btype->addParent(anyType);
                }
            }
        } catch (const InheritanceVisitorException& myException) {
            bmodule->reportTypeErrors();
            return false;
        }
    }

    return true;
}

bool BalancePackage::registerTypes(std::map<std::string, BalanceModule *> modules)
{
    for (auto const &x : modules) {
        BalanceModule *bmodule = x.second;
        if (bmodule->tree == nullptr) {
            continue;
        }
        try {
            this->currentModule = bmodule;

            // Visit entire tree
            TypeRegistrationVisitor visitor;
            visitor.visit(this->currentModule->tree);
        } catch (const TypeRegistrationVisitorException& myException) {
            bmodule->reportTypeErrors();
            return false;
        }
    }

    return true;
}

bool BalancePackage::registerGenericTypes(std::map<std::string, BalanceModule *> modules) {
    for (auto const &x : modules) {
        BalanceModule *bmodule = x.second;

        try {
            this->currentModule = bmodule;

            if (bmodule->tree != nullptr) {
                // Visit entire tree
                GenericTypeRegistrationVisitor visitor;
                visitor.visit(this->currentModule->tree);
            }

            // Replace generic types with concrete types which should be known by now.
            for (BalanceType * btype : bmodule->types) {
                if (btype->generics.size() > 0) {
                    BalanceType * baseType = bmodule->genericTypes[btype->name];
                    std::map<std::string, BalanceType *> typeMapping = {};
                    for (int i = 0; i < baseType->generics.size(); i++) {
                        typeMapping[baseType->generics[i]->name] = btype->generics[i];
                    }

                    btype->genericsMapping = typeMapping;
                }
            }
        } catch (const TypeRegistrationVisitorException& myException) {
            bmodule->reportTypeErrors();
            return false;
        }
    }

    return true;
}

// void BalancePackage::buildLanguageServerTokens() {
//     for (auto const &x : modules)
//     {
//         BalanceModule *bmodule = x.second;
//         this->currentModule = bmodule;

//         // Visit entire tree
//         TokenVisitor visitor;
//         visitor.visit(this->currentModule->tree);
//     }
// }

void BalancePackage::llvmCompile(std::map<std::string, BalanceModule *> modules)
{
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        if (bmodule->tree == nullptr) {
            continue;
        }

        currentPackage->currentModule = bmodule;

        // Visit entire tree
        BalanceVisitor visitor;
        visitor.visit(bmodule->tree);

        // Return 0
        currentPackage->currentModule->builder->CreateRet(ConstantInt::get(*currentPackage->context, APInt(32, 0)));
    }
}

BalanceModule *BalancePackage::getNextElementOrNull()
{
    for (auto const &x : modules)
    {
        BalanceModule *module = x.second;
        if (!module->finishedDiscovery)
        {
            return module;
        }
    }
    return nullptr;
}

void BalancePackage::buildDependencyTree(BalanceModule * bmodule)
{
    PackageVisitor visitor;
    BalanceModule *module = bmodule;
    this->modules[bmodule->name] = bmodule;
    while (module != nullptr)
    {
        this->currentModule = module;
        this->currentModule->generateAST();
        visitor.visit(module->tree);
        module->finishedDiscovery = true;
        module = getNextElementOrNull();
    }
}

bool BalancePackage::typeChecking(std::map<std::string, BalanceModule *> modules) {
    bool anyError = false;
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        if (bmodule->tree == nullptr) {
            continue;
        }

        this->currentModule = bmodule;

        // Visit entire tree
        TypeVisitor visitor;
        visitor.visit(bmodule->tree);

        if (bmodule->hasTypeErrors()) {
            anyError = true;
            bmodule->reportTypeErrors();
        }
    }
    return !anyError;
}

void BalancePackage::writeModuleToBinary(BalanceModule * bmodule) {
    auto TargetTriple = sys::getDefaultTargetTriple();

    std::string Error;
    auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);

    if (!Target)
    {
        errs() << Error;
        return;
    }

    auto CPU = "generic";
    auto Features = "";

    TargetOptions opt;
    auto RM = Optional<Reloc::Model>();
    auto TargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

    if (currentPackage->verboseLogging)
    {
        bmodule->module->print(llvm::errs(), nullptr);
    }

    bmodule->module->setDataLayout(TargetMachine->createDataLayout());
    bmodule->module->setTargetTriple(TargetTriple);

    std::filesystem::path p = this->buildDirectory / (bmodule->module->getSourceFileName() + ".o");

    if (!std::filesystem::is_directory(p.parent_path())) {
        std::filesystem::create_directories(p.parent_path());
    }

    std::string objectFileName = std::filesystem::absolute(p).string();
    std::error_code EC;
    raw_fd_ostream dest(objectFileName, EC, sys::fs::OF_None);

    if (EC)
    {
        errs() << EC.message();
        return;
    }

    legacy::PassManager pass;
    auto FileType = CGFT_ObjectFile;

    if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType))
    {
        errs() << "TargetMachine can't emit a file of this type";
        return;
    }
    pass.run(*bmodule->module);
    dest.flush();
}

void BalancePackage::writePackageToBinary(std::string entrypointName)
{
    auto TargetTriple = sys::getDefaultTargetTriple();
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    // Initialize build directory
    if (std::filesystem::is_directory(this->buildDirectory)) {
        std::filesystem::remove_all(this->buildDirectory);
    }
    std::filesystem::create_directories(this->buildDirectory);

    for (auto const &x : this->builtinModules)
    {
        BalanceModule *bmodule = x.second;
        writeModuleToBinary(bmodule);
    }

    for (auto const &x : this->modules)
    {
        BalanceModule *bmodule = x.second;
        writeModuleToBinary(bmodule);
    }

    IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts = new clang::DiagnosticOptions;
    clang::TextDiagnosticPrinter *DiagClient = new clang::TextDiagnosticPrinter(errs(), &*DiagOpts);
    IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new clang::DiagnosticIDs());
    clang::DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagClient);
    clang::driver::Driver TheDriver(CLANGXX, TargetTriple, Diags);

    std::vector<std::string> clangArguments = {"-g"};
    std::vector<std::string> objectFilePaths;

    // Add builtins
    for (auto const &x : builtinModules)
    {
        BalanceModule *bmodule = x.second;
        std::string objectFileName = this->buildDirectory / (bmodule->module->getSourceFileName() + ".o");
        objectFilePaths.push_back(objectFileName);
        clangArguments.push_back(objectFileName);
    }

    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        std::string objectFileName = this->buildDirectory / (bmodule->module->getSourceFileName() + ".o");
        objectFilePaths.push_back(objectFileName);
        clangArguments.push_back(objectFileName);
    }
    clangArguments.push_back("-o");
    clangArguments.push_back(entrypointName);

    std::vector<const char *> clangArgumentsCString;
    for (int i = 0; i < clangArguments.size(); ++i)
    {
        clangArgumentsCString.push_back(clangArguments[i].c_str());
    }
    auto args = ArrayRef<const char *>(clangArgumentsCString);

    std::unique_ptr<clang::driver::Compilation> C(TheDriver.BuildCompilation(args));

    if (C && !C->containsError())
    {
        SmallVector<std::pair<int, const clang::driver::Command *>, 4> FailingCommands;
        TheDriver.ExecuteCompilation(*C, FailingCommands);
    }
}