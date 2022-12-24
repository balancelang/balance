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

#include "config.h"

#include <map>
#include <queue>
#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"
#include <filesystem>

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
        std::string entrypointPath = this->packagePath + "/" + entrypointValue;
        this->entrypoints[entrypointName] = entrypointPath;

        // Check if entrypointValue exists?
        if (!fileExist(entrypointPath)) {
            std::cout << "Entrypoint does not exist: " << entrypointPath << std::endl;
            exit(1);
        }
    }

    // Dependencies
}

bool BalancePackage::execute()
{
    if (fileExist(this->packageJsonPath)) {
        // Load json file into properties
        this->load();
        this->populate();
    } else {
        // Find all balance scripts and add as entrypoints
        for (const auto & entry : fs::directory_iterator(this->packagePath)) {
            auto extension = entry.path().extension().string();
            if (extension == ".bl") {
                std::string filename = entry.path().filename().string();
                std::string filenameWithoutExtension = filename.substr(0, filename.find_last_of("."));
                this->entrypoints[filenameWithoutExtension] = entry.path();
            }
        }
    }

    return this->compileAndPersist();
}

bool BalancePackage::executeAsScript() {
    this->entrypoints["default"] = this->entrypoint;
    return this->compileAndPersist();
}

bool BalancePackage::compileAndPersist()
{
    bool compileSuccess = true;
    for (auto const &entryPoint : this->entrypoints)
    {
        // For now we reset and build each entryPoint from scratch. We can probably optimize that some day.
        this->reset();

        bool success = true;

        this->logger("Compiling entrypoint: " + entryPoint.second);

        // (PackageVisitor.cpp) Build import tree
        this->buildDependencyTree(entryPoint.second);

        this->logger("Creating builtins entrypoint");
        createBuiltins();

        // Add builtins to modules
        this->logger("Adding builtins to modules");
        this->addBuiltinsToModules();

        // TypeRegistrationVisitor.cpp - visit all types so they exist
        this->logger("Registering types");
        success = this->registerTypes();
        if (!success) {
            compileSuccess = false;
            continue;
        }

        // (StructureVisitor.cpp) Visit all class, class-methods and function definitions (textually only)
        this->logger("Building textual representations");
        success = this->buildTextualRepresentations();
        if (!success) {
            compileSuccess = false;
            continue;
        }

        // Type checking, also creates all class, class-methods and function definitions (textually only)
        this->logger("Running type checking");
        success = this->typeChecking();
        if (!success) {
            compileSuccess = false;
            continue;
        }

        if (this->isAnalyzeOnly) {
            continue;
        }

        // Run loop that builds LLVM functions and handles cycles
        this->buildStructures();

        // Build vtables for interfaces etc.
        this->buildVTables();

        // Make sure all classes have constructors (we need constructor function to make forward declaration)
        this->buildConstructors();

        // Make sure all modules have forward declarations of imported classes etc.
        this->buildForwardDeclarations();

        // (Visitor.cpp) Compile everything, now that functions, classes etc exist
        this->compile();

        // Persist modules as binary
        this->writePackageToBinary(entryPoint.first);
    }

    return compileSuccess;
}

// TODO: Combine this function and the above
bool BalancePackage::executeString(std::string program) {
    this->reset();

    bool success = true;
    currentPackage = this;
    modules["program"] = new BalanceModule("program", true);
    modules["program"]->initializeModule();
    modules["program"]->generateASTFromString(program);
    this->currentModule = modules["program"];

    createBuiltins();
    this->addBuiltinsToModules();

    this->logger("Registering types");
    success = this->registerTypes();
    if (!success) {
        return false;
    }

    // (StructureVisitor.cpp) Visit all class, class-methods and function definitions (textually only)
    success = this->buildTextualRepresentations();
    if (!success) {
        return false;
    }

    // Type checking, also creates all class, class-methods and function definitions (textually only)
    success = this->typeChecking();
    if (!success) {
        return false;
    }

    // Run loop that builds LLVM functions and handles cycles
    this->buildStructures();

    // Build vtables for interfaces etc.
    this->buildVTables();

    // Make sure all classes have constructors (we need constructor function to make forward declaration)
    this->buildConstructors();

    // Make sure all modules have forward declarations of imported classes etc.
    this->buildForwardDeclarations();

    // (Visitor.cpp) Compile everything, now that functions, classes etc exist
    this->compile();

    // Persist modules as binary
    this->writePackageToBinary("program");

    return true;
}

void BalancePackage::buildConstructors() {
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        this->currentModule = bmodule;

        // Visit entire tree
        ConstructorVisitor visitor;
        visitor.visit(bmodule->tree);
    }
}

void BalancePackage::buildStructures()
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

        // Visit entire tree
        LLVMTypeVisitor visitor;
        visitor.visit(bmodule->tree);

        bool isFinalized = true;

        // Check all types
        for (auto const &x : bmodule->types) {
            BalanceType * btype = x.second;

            if (!btype->finalized()) {
                isFinalized = false;
                break;
            }
        }

        // Check all functions
        for (auto const &x : bmodule->functions) {
            BalanceFunction * bfunction = x.second;
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

void BalancePackage::buildVTables() {
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        this->currentModule = bmodule;

        // Visit entire tree
        InterfaceVTableVisitor visitor;
        visitor.visit(bmodule->tree);
    }

    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        this->currentModule = bmodule;

        // Visit entire tree
        ClassVTableVisitor visitor;
        visitor.visit(bmodule->tree);
    }
}

void BalancePackage::buildForwardDeclarations()
{
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        this->currentModule = bmodule;

        // Visit entire tree
        ForwardDeclarationVisitor visitor;
        visitor.visit(bmodule->tree);
    }
}


void BalancePackage::addBuiltinsToModules() {
    BalanceModule * builtinsModule = this->builtins;

    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        this->currentModule = bmodule;

        for (auto const &x : builtinsModule->functions) {
            BalanceFunction * bfunction = x.second;
            createImportedFunction(bmodule, bfunction);
        }

        for (auto const &x : builtinsModule->types) {
            BalanceType * bclass = x.second;
            createImportedClass(bmodule, bclass);
        }
    }
}

bool BalancePackage::buildTextualRepresentations()
{
    for (auto const &x : modules) {
        BalanceModule *bmodule = x.second;
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

bool BalancePackage::registerTypes()
{
    for (auto const &x : modules) {
        BalanceModule *bmodule = x.second;
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

void BalancePackage::buildLanguageServerTokens() {
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        this->currentModule = bmodule;

        // Visit entire tree
        TokenVisitor visitor;
        visitor.visit(this->currentModule->tree);
    }
}

void BalancePackage::compile()
{
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
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

void BalancePackage::buildDependencyTree(std::string rootPath)
{
    std::string rootPathWithoutExtension = rootPath.substr(0, rootPath.find_last_of("."));
    modules[rootPathWithoutExtension] = new BalanceModule(rootPathWithoutExtension, true);
    modules[rootPathWithoutExtension]->initializeModule();
    this->currentModule = modules[rootPathWithoutExtension];
    this->currentModule->generateASTFromPath();
    modules[rootPathWithoutExtension]->finishedDiscovery = true;

    this->logger("Running PackageVisitor");
    PackageVisitor visitor;
    visitor.visit(this->currentModule->tree);

    this->logger("Found " + std::to_string(modules.size()) + " modules");
    if (modules.size() > 1)
    {
        BalanceModule *module = getNextElementOrNull();
        while (module != nullptr)
        {
            this->currentModule = module;
            this->currentModule->generateASTFromPath();
            visitor.visit(module->tree);
            module->finishedDiscovery = true;
            module = getNextElementOrNull();
        }
    }
}

bool BalancePackage::typeChecking() {
    bool anyError = false;
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
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

void writeModuleToBinary(BalanceModule * bmodule) {
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

    std::string objectFileName = bmodule->module->getSourceFileName() + ".o";
    std::error_code EC;
    raw_fd_ostream dest(objectFileName, EC, sys::fs::OF_None);

    if (EC)
    {
        errs() << "Could not open file: " << EC.message();
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

    writeModuleToBinary(this->builtins);
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
    objectFilePaths.push_back("builtins.o");
    clangArguments.push_back("builtins.o");

    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        std::string objectFileName = bmodule->module->getSourceFileName() + ".o";
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

    for (std::string objectFilePath : objectFilePaths)
    {
        remove(objectFilePath.c_str());
    }
}