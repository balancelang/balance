#include "../headers/TypeVisitor.h"

#include "../headers/Visitor.h"
#include "../headers/Package.h"
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

extern BalancePackage *currentPackage;

std::any TypeVisitor::visitImportStatement(BalanceParser::ImportStatementContext *ctx) {
    std::string text = ctx->getText();

    std::string importPath;
    if (ctx->IDENTIFIER()) {
        importPath = ctx->IDENTIFIER()->getText();
    } else if (ctx->IMPORT_PATH()) {
        importPath = ctx->IMPORT_PATH()->getText();
    }

    BalanceModule * importedModule = currentPackage->modules[importPath];

    for (BalanceParser::ImportDefinitionContext *parameter : ctx->importDefinitionList()->importDefinition())
    {
        if (dynamic_cast<BalanceParser::UnnamedImportDefinitionContext *>(parameter)) {
            BalanceParser::UnnamedImportDefinitionContext * import = dynamic_cast<BalanceParser::UnnamedImportDefinitionContext *>(parameter);
            std::string importString = import->IDENTIFIER()->getText();
            if (importedModule->classes.find(importString) != importedModule->classes.end()) {
                BalanceClass * bclass = importedModule->classes[importString];
                currentPackage->currentModule->importedClasses[importString] = bclass;
            } else if (importedModule->functions.find(importString) != importedModule->functions.end()) {
                BalanceFunction * bfunction = importedModule->functions[importString];
                currentPackage->currentModule->importedFunctions[importString] = bfunction;
            } else if (importedModule->globals[importString] != nullptr) {
                // TODO: Handle
            }
        }
    }

    return nullptr;
}

std::any TypeVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string text = ctx->getText();
    string className = ctx->className->getText();

    BalanceClass * bclass = currentPackage->currentModule->classes[className];
    // If already finalized, skip
    if (bclass->finalized()) {
        return nullptr;
    }

    currentPackage->currentModule->currentClass = bclass;

    StructType *structType = StructType::create(*currentPackage->currentModule->context, className);

    // Visit all class properties
    for (auto const &x : ctx->classElement())
    {
        if (x->classProperty())
        {
            visit(x->classProperty());
        }
    }

    // Visit all class functions
    for (auto const &x : ctx->classElement())
    {
        if (x->functionDefinition())
        {
            visit(x);
        }
    }

    vector<Type *> propertyTypes;
    for (auto const &x : currentPackage->currentModule->currentClass->properties)
    {
        BalanceProperty * prop = x.second;
        if (!prop->finalized()) {
            // If a property type is not yet known
            currentPackage->currentModule->currentClass = nullptr;
            return nullptr;
        }
        propertyTypes.push_back(prop->type);
    }

    currentPackage->currentModule->currentClass->structType = structType;
    ArrayRef<Type *> propertyTypesRef(propertyTypes);
    currentPackage->currentModule->currentClass->structType->setBody(propertyTypesRef, false);
    currentPackage->currentModule->currentClass->hasBody = true;

    currentPackage->currentModule->currentClass = nullptr;
    return nullptr;
}

std::any TypeVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) {
    string functionName = ctx->IDENTIFIER()->getText();

    BalanceFunction * bfunction;
    if (currentPackage->currentModule->currentClass != nullptr) {
        bfunction = currentPackage->currentModule->currentClass->methods[functionName];
    } else {
        bfunction = currentPackage->currentModule->functions[functionName];
    }

    for (BalanceParameter *bparameter : bfunction->parameters) {
        if (bparameter->type == nullptr) {
            Type *type = getBuiltinType(bparameter->typeString);
            if (type != nullptr) {
                bparameter->type = type;
            } else {
                BalanceClass * bclass = currentPackage->currentModule->getClass(bparameter->typeString);
                if (bclass == nullptr) {
                    bclass = currentPackage->currentModule->getImportedClass(bparameter->typeString);
                    if (bclass == nullptr) {
                        // TODO: Throw error
                    }
                }
                bparameter->type = bclass->structType;
            }
        }
    }

    // TODO: Handle classes from this or other module
    bfunction->returnType = getBuiltinType(bfunction->returnTypeString);

    return nullptr;
}

std::any TypeVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    string text = ctx->getText();
    string typeString = ctx->type->getText();
    string name = ctx->name->getText();

    BalanceProperty * bprop = currentPackage->currentModule->currentClass->properties[name];
    // If already finalized, skip
    if (bprop->finalized()) {
        return nullptr;
    }

    Type *typeValue = getBuiltinType(typeString);
    if (typeValue == nullptr)
    {
        BalanceClass * bclass = currentPackage->currentModule->importedClasses[typeString];
        if (bclass != nullptr && bclass->finalized()) {
            int count = currentPackage->currentModule->currentClass->properties.size();
            if (!bprop->finalized()) {
                bprop->type = typeValue;
            }
        }
    } else {
        bprop->type = typeValue;
    }

    return nullptr;
}
