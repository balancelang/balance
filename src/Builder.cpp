#include "headers/Builder.h"
#include "config.h"

using namespace llvm;

void writeModuleToFile(std::string fileName, std::vector<Module *> modules)
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

    for (Module * module : modules) {
        module->setDataLayout(TargetMachine->createDataLayout());
        module->setTargetTriple(TargetTriple);

        std::string objectFileName = module->getSourceFileName() + ".o";
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

        pass.run(*module);
        dest.flush();
    }

    IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts = new clang::DiagnosticOptions;
    clang::TextDiagnosticPrinter *DiagClient = new clang::TextDiagnosticPrinter(errs(), &*DiagOpts);
    IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new clang::DiagnosticIDs());
    clang::DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagClient);
    clang::driver::Driver TheDriver(CLANGXX, TargetTriple, Diags);

    std::vector<const char *> myArguments = { "-g" };
    for (Module * module : modules) {
        std::string objectFileName = module->getSourceFileName() + ".o";
        myArguments.push_back(objectFileName.c_str());
    }
    myArguments.push_back("-o");
    myArguments.push_back(fileName.c_str());
    auto args = ArrayRef<const char *>(myArguments);

    std::unique_ptr<clang::driver::Compilation> C(TheDriver.BuildCompilation(args));

    if (C && !C->containsError())
    {
        SmallVector<std::pair<int, const clang::driver::Command *>, 4> FailingCommands;
        TheDriver.ExecuteCompilation(*C, FailingCommands);
    }

    for (Module * module : modules) {
        std::string objectFileName = module->getSourceFileName() + ".o";
        remove(objectFileName.c_str());
    }
}