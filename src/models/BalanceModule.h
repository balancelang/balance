#ifndef BALANCE_MODULE_H
#define BALANCE_MODULE_H

#include "BalanceScopeBlock.h"
#include "BalanceClass.h"
#include "BalanceFunction.h"
#include "llvm/IR/IRBuilder.h"
#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "ParserRuleContext.h"

using namespace antlrcpptest;
using namespace antlr4;

class BalanceImportedClass;
class BalanceImportedFunction;
class BalanceClass;
class BalanceFunction;

class Position {
public:
    int line;
    int column;
    Position(int line, int column) {
        this->line = line;
        this->column = column;
    }
};

class Range {
public:
    Position * start;
    Position * end;
    Range(Position * start, Position * end) {
        this->start = start;
        this->end = end;
    }
};

class TypeError
{
public:
    Range * range;
    std::string message;
    TypeError(Range * range, std::string message) {
        this->range = range;
        this->message = message;
    }
};

class BalanceModule
{
public:
    llvm::IRBuilder<> *builder;

    std::string path;
    std::string filePath;
    std::string name;
    bool isEntrypoint;

    // Structures defined in this module
    std::map<std::string, BalanceClass *> classes = {};
    std::map<std::string, BalanceFunction *> functions = {};
    std::map<std::string, llvm::Value *> globals = {};

    // Structures imported into this module
    std::map<std::string, BalanceImportedClass *> importedClasses = {};
    std::map<std::string, BalanceImportedFunction *> importedFunctions = {};
    std::map<std::string, llvm::Value *> importedGlobals = {};

    BalanceScopeBlock *rootScope;
    BalanceClass *currentClass = nullptr;
    BalanceFunction *currentFunction = nullptr;  // TODO: Update visitor.cpp to use this?
    BalanceScopeBlock *currentScope;

    // Used to store e.g. 'x' in 'x.toString()', so we know 'toString()' is attached to x.
    llvm::Value *accessedValue = nullptr;

    llvm::Module *module;

    // The AST of the module
    antlr4::ANTLRInputStream * antlrStream = nullptr;
    antlr4::CommonTokenStream * tokenStream = nullptr;
    BalanceLexer * lexer = nullptr;
    BalanceParser * parser = nullptr;
    antlr4::tree::ParseTree *tree = nullptr;

    // Typechecking
    std::vector<TypeError *> typeErrors;

    bool finishedDiscovery;

    BalanceModule(string path, bool isEntrypoint)
    {
        this->path = path;
        this->isEntrypoint = isEntrypoint;
        this->filePath = this->path + ".bl";

        if (this->path.find('/') != std::string::npos)
        {
            this->name = this->path.substr(this->path.find_last_of("/"), this->path.size());
        }
        else
        {
            this->name = this->path;
        }

        this->initializeModule();
    }

    void addTypeError(ParserRuleContext * ctx, std::string message);
    void initializeModule();
    void generateASTFromStream(antlr4::ANTLRInputStream * stream);
    void generateASTFromPath(std::string filePath);
    void generateASTFromString(std::string program);
    BalanceClass * getClass(BalanceTypeString * className);
    BalanceClass * getClassFromStructName(std::string structName);
    BalanceClass * getClassFromBaseName(std::string baseName);
    BalanceImportedClass * getImportedClass(BalanceTypeString * className);
    BalanceImportedClass * getImportedClassFromStructName(std::string structName);
    BalanceImportedClass * getImportedClassFromBaseName(std::string baseName);

    BalanceFunction * getFunction(std::string functionName);
    BalanceImportedFunction * getImportedFunction(std::string functionName);

    BalanceTypeString *getTypeValue(std::string variableName);
    llvm::Value *getValue(std::string variableName);
    void setValue(std::string variableName, llvm::Value *value);
    bool finalized();

    bool hasTypeErrors();
    void reportTypeErrors();
};

#endif