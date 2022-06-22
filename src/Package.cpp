#include "headers/Package.h"
#include "headers/Main.h"
#include "headers/Utilities.h"
#include "headers/ImportVisitor.h"
#include "config.h"

#include <map>
#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"

using namespace std;

extern LLVMContext *context;
extern BalanceModule *currentModule;
extern std::unique_ptr<IRBuilder<>> builder;
extern bool verbose;

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

        // TODO: Check if entrypointValue exists?
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

bool BalancePackage::compileAndPersist()
{
    for (auto const &entryPoint : this->entrypoints)
    {
        // Build import tree
        this->buildDependencyTree(entryPoint.second);

        this->compileRecursively(entryPoint.second);

        // Persist modules as binary
        this->writePackageToBinary(entryPoint.first);

        // TODO: For now we just build the first entrypoint
        break;
    }

    return true;
}

void BalancePackage::writePackageToBinary(std::string entrypointName) {
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

    for (auto const &x : modules) {
        BalanceModule * bmodule = x.second;
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

    std::vector<const char *> myArguments = { "-g" };
    std::vector<std::string> objectFilePaths;
    for (auto const &x : modules) {
        BalanceModule * bmodule = x.second;
        std::string objectFileName = bmodule->module->getSourceFileName() + ".o";
        objectFilePaths.push_back(objectFileName);
        myArguments.push_back(objectFileName.c_str());
    }
    myArguments.push_back("-o");
    myArguments.push_back(entrypointName.c_str());
    auto args = ArrayRef<const char *>(myArguments);

    std::unique_ptr<clang::driver::Compilation> C(TheDriver.BuildCompilation(args));

    if (C && !C->containsError())
    {
        SmallVector<std::pair<int, const clang::driver::Command *>, 4> FailingCommands;
        TheDriver.ExecuteCompilation(*C, FailingCommands);
    }

    for (std::string objectFilePath : objectFilePaths) {
        remove(objectFilePath.c_str());
    }
}

void buildModuleFromStream(BalanceModule * bmodule, ANTLRInputStream stream)
{
    initializeModule(bmodule);
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
    builder->CreateRet(ConstantInt::get(*context, APInt(32, 0)));
}

void buildModuleFromString(BalanceModule * bmodule, string program)
{
    ANTLRInputStream input(program);
    return buildModuleFromStream(bmodule, input);
}

void buildModuleFromPath(BalanceModule * bmodule)
{
    ifstream inputStream;
    inputStream.open(bmodule->filePath);

    ANTLRInputStream input(inputStream);
    buildModuleFromStream(bmodule, input);
}

void BalancePackage::compileRecursively(std::string path)
{
    for (auto const &x : modules)
    {
        BalanceModule *balanceModule = x.second;
        buildModuleFromPath(balanceModule);
    }
}

BalanceModule * BalancePackage::getNextElementOrNull()
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
    initializeModule(modules[rootPathWithoutExtension]);
    ifstream inputStream;
    inputStream.open(rootPath);

    ANTLRInputStream input(inputStream);
    BalanceLexer lexer(&input);
    CommonTokenStream tokens(&lexer);

    tokens.fill();

    BalanceParser parser(&tokens);
    tree::ParseTree *tree = parser.root();

    modules[rootPathWithoutExtension]->finishedDiscovery = true;
    BalanceImportVisitor visitor;
    visitor.visit(tree);

    if (modules.size() > 1)
    {
        BalanceModule *module = getNextElementOrNull();
        while (module != nullptr)
        {
            initializeModule(module);
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
