#include "ForwardDeclarationVisitor.h"

#include "Visitor.h"
#include "../BalancePackage.h"
#include "../Utilities.h"
#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

#include "clang/Driver/Driver.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Driver/Compilation.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "antlr4-runtime.h"

#include "config.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeinfo>
#include <typeindex>

using namespace antlrcpptest;
using namespace std;

extern BalancePackage *currentPackage;


// TODO: This should also create forward declarations of classes so order doesn't matter


std::any ForwardDeclarationVisitor::visitImportStatement(BalanceParser::ImportStatementContext *ctx) {
    std::string text = ctx->getText();

    std::string importPath;
    if (ctx->IDENTIFIER()) {
        importPath = ctx->IDENTIFIER()->getText();
    } else if (ctx->IMPORT_PATH()) {
        importPath = ctx->IMPORT_PATH()->getText();
    } else {
        // TODO: Handle this with an error
    }

    BalanceModule * importedModule = currentPackage->modules[importPath];

    for (BalanceParser::ImportDefinitionContext *parameter : ctx->importDefinitionList()->importDefinition())
    {
        if (dynamic_cast<BalanceParser::UnnamedImportDefinitionContext *>(parameter)) {
            BalanceParser::UnnamedImportDefinitionContext * import = dynamic_cast<BalanceParser::UnnamedImportDefinitionContext *>(parameter);
            std::string importString = import->IDENTIFIER()->getText();

            BalanceFunction * bfunction = importedModule->getFunction(importString);
            if (bfunction != nullptr) {
                createImportedFunction(currentPackage->currentModule, bfunction);
            } else {
                BalanceType * btype = importedModule->getType(new BalanceTypeString(importString));
                if (btype == nullptr) {
                    // TODO: Handle
                }

                if (btype->isInterface()) {
                    // TODO: Handle importing interfaces
                } else {
                    createImportedClass(currentPackage->currentModule, (BalanceClass *) btype);
                }
            }
        }
    }

    return nullptr;
}