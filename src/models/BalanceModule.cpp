#include "BalanceModule.h"
#include "BalanceValue.h"
#include "../BalancePackage.h"

#include "ParserRuleContext.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

void BalanceModule::initializeModule() {
    this->builder = new llvm::IRBuilder<>(*currentPackage->context);
    this->module = new llvm::Module(this->path, *currentPackage->context);

    // Initialize module root scope
    FunctionType *funcType = FunctionType::get(this->builder->getInt32Ty(), false);
    std::string rootFunctionName = this->path + "_main";
    Function *rootFunc = Function::Create(funcType, Function::ExternalLinkage, this->isEntrypoint ? "main" : rootFunctionName, this->module);
    BasicBlock *entry = BasicBlock::Create(*currentPackage->context, "entrypoint", rootFunc);
    this->builder->SetInsertPoint(entry);
    this->rootScope = new BalanceScopeBlock(entry, nullptr);
    this->currentScope = this->rootScope;
}

void BalanceModule::generateASTFromStream(antlr4::ANTLRInputStream *stream) {
    this->antlrStream = stream;
    this->lexer = new BalanceLexer(this->antlrStream);
    this->tokenStream = new antlr4::CommonTokenStream(this->lexer);
    this->tokenStream->fill();
    this->parser = new BalanceParser(this->tokenStream);
    this->tree = this->parser->root();
}

void BalanceModule::generateASTFromPath() {
    ifstream inputStream;
    inputStream.open(this->filePath);
    this->antlrStream = new antlr4::ANTLRInputStream(inputStream);
    this->generateASTFromStream(antlrStream);
}

void BalanceModule::generateASTFromString(std::string program) {
    this->antlrStream = new antlr4::ANTLRInputStream(program);
    this->generateASTFromStream(antlrStream);
}

// TODO: Should this take a std::string instead? Do we ever need the generics when retrieving a type?
BalanceType * BalanceModule::getType(BalanceTypeString * typeName) {
    for (auto const &x : classes)
    {
        BalanceClass * bclass = x.second;
        if (bclass->name->equalTo(typeName)) {
            return bclass;
        }
    }

    for (auto const &x : interfaces)
    {
        BalanceInterface * binterface = x.second;
        if (binterface->name->base == typeName->base) {
            return binterface;
        }
    }

    for (auto const &x : importedClasses)
    {
        BalanceClass * bclass = x.second;
        if (bclass->name->equalTo(typeName)) {
            return bclass;
        }
    }

    // for (auto const &x : importedInterfaces)
    // {
    //     BalanceClass * bclass = x.second;
    //     if (bclass->name->base == className->base) {
    //         return bclass;
    //     }
    // }

    return nullptr;
}

BalanceFunction *BalanceModule::getFunction(std::string functionName) {
    for (auto const &x : functions)
    {
        BalanceFunction * bfunction = x.second;
        if (bfunction->name == functionName) {
            return bfunction;
        }
    }

    for (auto const &x : importedFunctions)
    {
        BalanceFunction * bfunction = x.second;
        if (bfunction->name == functionName) {
            return bfunction;
        }
    }

    return nullptr;
}

BalanceTypeString *BalanceModule::getTypeValue(std::string variableName) {
    BalanceScopeBlock *scope = this->currentScope;
    while (scope != nullptr) {
        // Check variables etc
        BalanceTypeString *tryVal = scope->typeSymbolTable[variableName];
        if (tryVal != nullptr) {
            return tryVal;
        }

        scope = scope->parent;
    }

    return nullptr;
}

BalanceValue *BalanceModule::getValue(std::string variableName) {
    BalanceScopeBlock *scope = this->currentScope;
    while (scope != nullptr) {
        // Check variables etc
        BalanceValue *tryVal = scope->symbolTable[variableName];
        if (tryVal != nullptr) {
            return tryVal;
        }

        scope = scope->parent;
    }

    return nullptr;
}

void BalanceModule::setValue(std::string variableName, BalanceValue *bvalue) {
    this->currentScope->symbolTable[variableName] = bvalue;
}

bool BalanceModule::finalized() {
    for (auto const &x : this->interfaces) {
        if (!x.second->finalized()) {
            return false;
        }
    }

    for (auto const &x : this->classes) {
        if (!x.second->finalized()) {
            return false;
        }
    }

    for (auto const &x : this->functions) {
        if (!x.second->finalized()) {
            return false;
        }
    }

    return true;
}

void BalanceModule::addTypeError(ParserRuleContext * ctx, std::string message) {
    Position * start = new Position(ctx->getStart()->getLine(), ctx->getStart()->getCharPositionInLine());

    int length = ctx->getStop()->getStopIndex() - ctx->getStop()->getStartIndex();
    int endIndex = ctx->getStop()->getCharPositionInLine() + length;
    Position * end = new Position(ctx->getStop()->getLine(), endIndex);

    Range * range = new Range(start, end);
    TypeError * typeError = new TypeError(range, message);
    this->typeErrors.push_back(typeError);
}

bool BalanceModule::hasTypeErrors() {
    return !this->typeErrors.empty();
}

void BalanceModule::reportTypeErrors() {
    for (TypeError * typeError : this->typeErrors) {
        std::cout << "Type error: " + typeError->message + ", on line " + std::to_string(typeError->range->start->line) << std::endl;
    }
}
