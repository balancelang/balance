#ifndef VISITOR_H
#define VISITOR_H

#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "Utilities.h"

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

#include "llvm/IR/BasicBlock.h"

using namespace antlrcpptest;
using namespace llvm;
using namespace std;

class ScopeBlock;
class BalancePackage;
class BalanceClass;
class BalanceFunction;
class BalanceImportedFunction;
class BalanceModule;
class BalanceProperty;

extern BalancePackage *currentPackage;
llvm::Value *anyToValue(any anyVal);
Type *getBuiltinType(string typeString);
Constant *geti8StrVal(Module &M, char const *str, Twine const &name);
void LogError(string errorMessage);

class ScopeBlock
{
public:
    BasicBlock *block;
    ScopeBlock *parent;
    map<string, llvm::Value *> symbolTable;

    ScopeBlock(BasicBlock *block, ScopeBlock *parent)
    {
        this->block = block;
        this->parent = parent;
    }
};

class BalanceProperty
{
public:
    string name;
    string stringType; // TODO: Make sure we use the same name convention (BalanceParameter calls it typeString)
    int index;
    Type *type;
    BalanceProperty(string name, string stringType, int index)
    {
        this->name = name;
        this->stringType = stringType;
        this->index = index;
        this->type = nullptr;
    }

    bool finalized()
    {
        return this->type != nullptr;
    }
};

class BalanceParameter
{
public:
    string typeString;
    string name;
    Type *type = nullptr;

    BalanceParameter(string typeString, string name)
    {
        this->typeString = typeString;
        this->name = name;
    }

    bool finalized()
    {
        return this->type != nullptr;
    }
};

class BalanceFunction
{
public:
    string name;
    string returnTypeString;
    vector<BalanceParameter *> parameters;
    Type *returnType = nullptr;
    Function *function = nullptr;
    // BalanceModule *module;

    BalanceFunction(string name, vector<BalanceParameter *> parameters, string returnTypeString)
    {
        this->name = name;
        this->parameters = parameters;
        this->returnTypeString = returnTypeString;
    }

    bool hasAllTypes() {
        for (auto const &x : this->parameters)
        {
            if (!x->finalized())
            {
                return false;
            }
        }

        if (returnType == nullptr)
        {
            return false;
        }

        return true;
    }

    bool finalized()
    {
        if (!this->hasAllTypes()) {
            return false;
        }

        if (function == nullptr) {
            return false;
        }

        return true;
    }
};

class BalanceImportedFunction {
public:
    BalanceModule * module;                 // The module importing the function
    BalanceFunction * bfunction;            // The source function
    llvm::Function * function = nullptr;    // The re-declared function in the module

    BalanceImportedFunction(BalanceModule * module, BalanceFunction * bfunction) {
        this->module = module;
        this->bfunction = bfunction;
    }
};

class BalanceImportedConstructor {
public:
    BalanceModule * module;
    BalanceClass * bclass;
    llvm::Function * constructor = nullptr;

    BalanceImportedConstructor(BalanceModule * module, BalanceClass * bclass) {
        this->module = module;
        this->bclass = bclass;
    }
};

class BalanceClass
{
public:
    string name;
    map<string, BalanceProperty *> properties = {};
    map<string, BalanceFunction *> methods = {};
    Function *constructor = nullptr;
    StructType *structType = nullptr;
    bool hasBody;
    BalanceModule *module;
    BalanceClass(string name)
    {
        this->name = name;
    }

    bool finalized()
    {
        bool empty = this->properties.empty();
        for (auto const &x : this->properties)
        {
            if (!x.second->finalized())
            {
                return false;
            }
        }

        for (auto const &x : this->methods)
        {
            if (!x.second->finalized())
            {
                return false;
            }
        }

        if (!this->hasBody)
        {
            return false;
        }

        return true;
    }
};

class BalanceImportedClass {
public:
    BalanceModule * module;                                 // The module importing the function
    BalanceClass * bclass;                                  // The source class
    map<string, BalanceImportedFunction *> methods = {};    // The re-declared methods in the class
    BalanceImportedConstructor * constructor = nullptr;

    BalanceImportedClass(BalanceModule * module, BalanceClass * bclass) {
        this->module = module;
        this->bclass = bclass;
    }
};

class BalanceModule
{
public:
    IRBuilder<> *builder;

    string path;
    string filePath;
    string name;
    bool isEntrypoint;

    // Structures defined in this module
    map<string, BalanceClass *> classes = {};
    map<string, BalanceFunction *> functions = {};
    map<string, llvm::Value *> globals = {};

    // Structures imported into this module
    map<string, BalanceImportedClass *> importedClasses = {};
    map<string, BalanceImportedFunction *> importedFunctions = {};
    map<string, llvm::Value *> importedGlobals = {};

    ScopeBlock *rootScope;
    BalanceClass *currentClass = nullptr;
    ScopeBlock *currentScope;

    llvm::Module *module;

    // The AST of the module
    antlr4::ANTLRInputStream * antlrStream = nullptr;
    antlr4::CommonTokenStream * tokenStream = nullptr;
    BalanceLexer * lexer = nullptr;
    BalanceParser * parser = nullptr;
    antlr4::tree::ParseTree *tree = nullptr;

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

    void initializeModule();

    void generateASTFromStream(antlr4::ANTLRInputStream * stream) {
        this->antlrStream = stream;
        this->lexer = new BalanceLexer(this->antlrStream);
        this->tokenStream = new antlr4::CommonTokenStream(this->lexer);
        this->tokenStream->fill();
        this->parser = new BalanceParser(this->tokenStream);
        this->tree = this->parser->root();
    }

    void generateASTFromPath(std::string filePath) {
        if (!fileExist(filePath)) {
            cout << "Input file doesn't exist: " << filePath << endl;
            exit(1);
        }

        ifstream inputStream;
        inputStream.open(filePath);
        this->antlrStream = new antlr4::ANTLRInputStream(inputStream);
        this->generateASTFromStream(antlrStream);
    }

    void generateASTFromString(std::string program) {
        this->antlrStream = new antlr4::ANTLRInputStream(program);
        this->generateASTFromStream(antlrStream);
    }

    BalanceClass * getClass(std::string className) {
        if (this->classes.find(className) != this->classes.end()) {
            return this->classes[className];
        }
        return nullptr;
    }

    BalanceFunction * getFunction(std::string functionName) {
        if (this->functions.find(functionName) != this->functions.end())
        {
            return this->functions[functionName];
        }
        return nullptr;
    }

    BalanceImportedClass * getImportedClass(std::string className) {
        if (this->importedClasses.find(className) != this->importedClasses.end()) {
            return this->importedClasses[className];
        }
        return nullptr;
    }

    BalanceImportedFunction * getImportedFunction(std::string functionName) {
        if (this->importedFunctions.find(functionName) != this->importedFunctions.end())
        {
            return this->importedFunctions[functionName];
        }
        return nullptr;
    }

    llvm::Value *getValue(std::string variableName)
    {
        ScopeBlock *scope = this->currentScope;
        while (scope != nullptr)
        {
            // Check variables etc
            llvm::Value *tryVal = scope->symbolTable[variableName];
            if (tryVal != nullptr)
            {
                return tryVal;
            }

            scope = scope->parent;
        }

        return nullptr;
    }

    void setValue(std::string variableName, llvm::Value *value)
    {
        this->currentScope->symbolTable[variableName] = value;
    }

    bool finalized()
    {
        for (auto const &x : this->classes)
        {
            if (!x.second->finalized())
            {
                return false;
            }
        }

        for (auto const &x : this->functions)
        {
            if (!x.second->finalized())
            {
                return false;
            }
        }

        return true;
    }
};

class BalanceVisitor : public BalanceParserBaseVisitor
{
public:
    any visitWhileStatement(BalanceParser::WhileStatementContext *ctx) override;
    any visitMemberAssignment(BalanceParser::MemberAssignmentContext *ctx) override;
    any visitMemberIndexExpression(BalanceParser::MemberIndexExpressionContext *ctx) override;
    any visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) override;
    any visitIfStatement(BalanceParser::IfStatementContext *ctx) override;
    any visitArgument(BalanceParser::ArgumentContext *ctx) override;
    any visitVariableExpression(BalanceParser::VariableExpressionContext *ctx) override;
    any visitNewAssignment(BalanceParser::NewAssignmentContext *ctx) override;
    any visitExistingAssignment(BalanceParser::ExistingAssignmentContext *ctx) override;
    any visitRelationalExpression(BalanceParser::RelationalExpressionContext *ctx) override;
    any visitAdditiveExpression(BalanceParser::AdditiveExpressionContext *ctx) override;
    any visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) override;
    any visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) override;
    any visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) override;
    any visitStringLiteral(BalanceParser::StringLiteralContext *ctx) override;
    any visitFunctionCall(BalanceParser::FunctionCallContext *ctx) override;
    any visitReturnStatement(BalanceParser::ReturnStatementContext *ctx) override;
    any visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) override;
    any visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx) override;
    any visitMemberAccessExpression(BalanceParser::MemberAccessExpressionContext *ctx) override;
    any visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) override;
    any visitClassInitializerExpression(BalanceParser::ClassInitializerExpressionContext *ctx) override;
    any visitMultiplicativeExpression(BalanceParser::MultiplicativeExpressionContext *ctx) override;
};

#endif