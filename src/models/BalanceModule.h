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

    std::string path;
    std::string filePath;
    std::string name;
    bool isEntrypoint;

    // Structures defined in this module
    std::vector<BalanceType *> types = {};
    std::map<std::string, BalanceFunction *> functions = {};
    std::map<std::string, llvm::Value *> globals = {};

    std::map<std::string, BalanceType *> genericTypes = {};

    // Structures imported into this module
    std::map<std::string, BalanceFunction *> importedFunctions = {};
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

    BalanceModule(std::string path, bool isEntrypoint)
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
    }

    void initializeTypeInfoTable();
    void addTypeError(ParserRuleContext * ctx, std::string message);
    void initializeModule();
    void generateASTFromStream(antlr4::ANTLRInputStream * stream);
    void generateASTFromPath();
    void generateASTFromString(std::string program);
    BalanceType * getType(std::string typeName, std::vector<BalanceType *> generics = {});
    void addType(BalanceType * balanceType);
    BalanceFunction * getFunction(std::string functionName);
    BalanceValue *getValue(std::string variableName);
    void setValue(std::string variableName, BalanceValue *bvalue);
    bool hasTypeErrors();
    void reportTypeErrors();
};

#endif