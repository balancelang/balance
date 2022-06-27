#include "headers/Package.h"
#include "headers/Main.h"
#include "headers/Utilities.h"
#include "headers/PackageVisitor.h"
#include "headers/StructureVisitor.h"
#include "headers/ForwardDeclarationVisitor.h"
#include "headers/ConstructorVisitor.h"
#include "headers/TypeVisitor.h"
#include "config.h"

#include <map>
#include <queue>
#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"

extern bool verbose;
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
        this->entrypoints[entrypointName] = entrypointValue;

        // Check if entrypointValue exists?
        if (!fileExist(entrypointValue)) {
            std::cout << "Entrypoint does not exist: " << entrypointValue << std::endl;
            exit(1);
        }
    }

    // Dependencies
}

bool BalancePackage::execute()
{
    // Load json file into properties
    this->load();
    this->populate();

    return this->compileAndPersist();
}

bool BalancePackage::executeAsScript() {
    this->entrypoints["default"] = this->entrypoint;
    return this->compileAndPersist();
}

bool BalancePackage::compileAndPersist()
{
    for (auto const &entryPoint : this->entrypoints)
    {
        // (PackageVisitor.cpp) Build import tree
        this->buildDependencyTree(entryPoint.second);

        // (StructureVisitor.cpp) Visit all class, class-methods and function definitions (textually only)
        this->buildTextualRepresentations();

        // Run loop that builds LLVM functions and handles cycles
        this->buildStructures();

        // Make sure all classes have constructors (we need constructor function to make forward declaration)
        this->buildConstructors();

        // Make sure all modules have forward declarations of imported classes etc.
        this->buildForwardDeclarations();

        // (Visitor.cpp) Compile everything, now that functions, classes etc exist
        this->compile();

        // Persist modules as binary
        this->writePackageToBinary(entryPoint.first);

        // For now we reset and build each entryPoint from scratch. We can probably optimize that some day.
        this->reset();
    }

    return true;
}

void BalancePackage::buildConstructors() {
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        this->currentModule = bmodule;
        ifstream inputStream;
        inputStream.open(bmodule->filePath);

        ANTLRInputStream stream(inputStream);
        BalanceLexer lexer(&stream);
        CommonTokenStream tokens(&lexer);

        tokens.fill();

        BalanceParser parser(&tokens);
        tree::ParseTree *tree = parser.root();

        if (verbose)
        {
            cout << tree->toStringTree(&parser, true) << endl;
        }

        // Visit entire tree
        ConstructorVisitor visitor;
        visitor.visit(tree);
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

        ifstream inputStream;
        inputStream.open(bmodule->filePath);

        ANTLRInputStream stream(inputStream);
        BalanceLexer lexer(&stream);
        CommonTokenStream tokens(&lexer);

        tokens.fill();

        BalanceParser parser(&tokens);
        tree::ParseTree *tree = parser.root();

        if (verbose)
        {
            cout << tree->toStringTree(&parser, true) << endl;
        }

        // Visit entire tree
        TypeVisitor visitor;
        visitor.visit(tree);

        bool isFinalized = bmodule->finalized();
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

void BalancePackage::buildForwardDeclarations()
{
    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;
        this->currentModule = bmodule;
        ifstream inputStream;
        inputStream.open(bmodule->filePath);

        ANTLRInputStream stream(inputStream);
        BalanceLexer lexer(&stream);
        CommonTokenStream tokens(&lexer);

        tokens.fill();

        BalanceParser parser(&tokens);
        tree::ParseTree *tree = parser.root();

        if (verbose)
        {
            cout << tree->toStringTree(&parser, true) << endl;
        }

        // Visit entire tree
        ForwardDeclarationVisitor visitor;
        visitor.visit(tree);
    }
}

void BalancePackage::buildTextualRepresentations()
{
    for (auto const &x : modules)
    {
        BalanceModule *balanceModule = x.second;
        this->currentModule = balanceModule;
        ifstream inputStream;
        inputStream.open(balanceModule->filePath);

        ANTLRInputStream stream(inputStream);
        BalanceLexer lexer(&stream);
        CommonTokenStream tokens(&lexer);

        tokens.fill();

        BalanceParser parser(&tokens);
        tree::ParseTree *tree = parser.root();

        if (verbose)
        {
            cout << tree->toStringTree(&parser, true) << endl;
        }

        // Visit entire tree
        StructureVisitor visitor;
        visitor.visit(tree);
    }
}

void buildModuleFromStream(BalanceModule *bmodule, ANTLRInputStream stream)
{
    currentPackage->currentModule = bmodule;
    BalanceLexer lexer(&stream);
    CommonTokenStream tokens(&lexer);

    tokens.fill();

    BalanceParser parser(&tokens);
    tree::ParseTree *tree = parser.root();

    if (verbose)
    {
        cout << tree->toStringTree(&parser, true) << endl;
    }

    create_functions();

    // Visit entire tree
    BalanceVisitor visitor;
    visitor.visit(tree);

    // Return 0
    currentPackage->currentModule->builder->CreateRet(ConstantInt::get(*currentPackage->currentModule->context, APInt(32, 0)));
}

void buildModuleFromString(BalanceModule *bmodule, std::string program)
{
    ANTLRInputStream input(program);
    return buildModuleFromStream(bmodule, input);
}

void buildModule(BalanceModule *bmodule)
{
    ifstream inputStream;
    inputStream.open(bmodule->filePath);

    ANTLRInputStream input(inputStream);
    buildModuleFromStream(bmodule, input);
}

void BalancePackage::compile()
{
    for (auto const &x : modules)
    {
        BalanceModule *balanceModule = x.second;
        buildModule(balanceModule);
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
    this->currentModule = modules[rootPathWithoutExtension];
    ifstream inputStream;
    inputStream.open(rootPath);

    ANTLRInputStream input(inputStream);
    BalanceLexer lexer(&input);
    CommonTokenStream tokens(&lexer);

    tokens.fill();

    BalanceParser parser(&tokens);
    tree::ParseTree *tree = parser.root();

    modules[rootPathWithoutExtension]->finishedDiscovery = true;
    PackageVisitor visitor;
    visitor.visit(tree);

    if (modules.size() > 1)
    {
        BalanceModule *module = getNextElementOrNull();
        while (module != nullptr)
        {
            this->currentModule = module;
            ifstream inputStream;
            inputStream.open(module->filePath);

            ANTLRInputStream input(inputStream);
            BalanceLexer lexer(&input);
            CommonTokenStream tokens(&lexer);

            tokens.fill();

            BalanceParser parser(&tokens);
            tree::ParseTree *tree = parser.root();
            visitor.visit(tree);
            module->finishedDiscovery = true;
            module = getNextElementOrNull();
        }
    }
}

void BalancePackage::writePackageToBinary(std::string entrypointName)
{
    auto TargetTriple = sys::getDefaultTargetTriple();
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

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

    for (auto const &x : modules)
    {
        BalanceModule *bmodule = x.second;

        if (verbose)
        {
            bmodule->module->print(llvm::errs(), nullptr);
        }

        this->currentModule = bmodule;
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

    IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts = new clang::DiagnosticOptions;
    clang::TextDiagnosticPrinter *DiagClient = new clang::TextDiagnosticPrinter(errs(), &*DiagOpts);
    IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new clang::DiagnosticIDs());
    clang::DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagClient);
    clang::driver::Driver TheDriver(CLANGXX, TargetTriple, Diags);

    std::vector<std::string> clangArguments = {"-g"};
    std::vector<std::string> objectFilePaths;
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