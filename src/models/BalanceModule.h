#ifndef BALANCE_MODULE_H
#define BALANCE_MODULE_H

#include "BalanceScopeBlock.h"
#include "BalanceType.h"
#include "BalanceFunction.h"
#include "BalanceLambda.h"
#include "BalanceValue.h"
#include "llvm/IR/IRBuilder.h"
#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "ParserRuleContext.h"
#include "BalanceSource.h"

#include <filesystem>

using namespace antlrcpptest;
using namespace antlr4;

class BalanceType;
class BalanceFunction;
class BalanceLambda;
class BalanceValue;

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

    // filePath relative to package root, without file extension
    std::string name;
    // absolute file path
    std::filesystem::path filePath;
    bool isEntrypoint;
    std::vector<BalanceSource *> sources = {};

    // Structures defined in this module
    std::vector<BalanceType *> types = {};
    std::vector<BalanceFunction *> functions = {};
    std::map<std::string, llvm::Value *> globals = {};

    std::map<std::string, BalanceType *> genericTypes = {};

    // Structures imported into this module
    std::vector<BalanceFunction *> importedFunctions = {};
    std::map<std::string, llvm::Value *> importedGlobals = {};

    BalanceScopeBlock *rootScope;
    BalanceType *currentType = nullptr;
    BalanceFunction *currentFunction = nullptr;     // Used by TypeVisitor.cpp
    BalanceLambda *currentLambda = nullptr;         // Used by TypeVisitor.cpp
    BalanceScopeBlock *currentScope;
    BalanceType * currentLhsType = nullptr;

    llvm::GlobalVariable * typeInfoTable = nullptr;
    llvm::StructType * typeInfoStructType = nullptr;

    // Used to store e.g. 'x' in 'x.toString()', so we know 'toString()' is attached to x.
    BalanceValue * accessedValue = nullptr;

    BalanceType * accessedType = nullptr;     // Used by TypeVisitor.cpp

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

    BalanceModule(std::string name, std::vector<BalanceSource *> sources, bool isEntrypoint)
    {
        this->name = name;
        this->sources = sources;
        this->filePath = filePath;
        this->isEntrypoint = isEntrypoint;
        this->initializeModule();
    }

    void initializeTypeInfoTable();
    void addFunction(BalanceFunction * function);
    void addTypeError(ParserRuleContext * ctx, std::string message);
    void initializeModule();
    void generateAST();
    BalanceType * getType(std::string typeName, std::vector<BalanceType *> generics);
    BalanceType * getType(std::string typeName);
    BalanceType * createGenericType(std::string typeName, std::vector<BalanceType *> generics);
    void addType(BalanceType * balanceType);
    std::vector<BalanceType *> getGenericVariants(std::string typeName);
    BalanceFunction * getFunction(std::string functionName, std::vector<BalanceType *> parameters);
    std::vector<BalanceFunction *> getFunctionsByName(std::string functionName);
    BalanceValue *getValue(std::string variableName);
    void setValue(std::string variableName, BalanceValue *bvalue);
    bool hasTypeErrors();
    void reportTypeErrors();
};

#endif