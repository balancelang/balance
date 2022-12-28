#include "BalanceModule.h"
#include "BalanceValue.h"
#include "../builtins/Array.h"
#include "../BalancePackage.h"

#include "llvm/IR/Value.h"
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

    this->initializeTypeInfoTable();
}

void BalanceModule::initializeTypeInfoTable() {
    llvm::StructType *structType = llvm::StructType::create(*currentPackage->context, "TypeInfo");
    llvm::ArrayRef<llvm::Type *> propertyTypesRef({
        Type::getInt32Ty(*currentPackage->context),
        this->builder->CreateGlobalStringPtr("")->getType()
    });
    structType->setBody(propertyTypesRef, false);

    this->typeInfoStructType = structType;

    llvm::ArrayType * arrayType = llvm::ArrayType::get(structType, 0);
    this->typeInfoTable = new llvm::GlobalVariable(*this->module, arrayType, true, llvm::GlobalValue::ExternalLinkage, nullptr, "typeTable");
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

BalanceType * BalanceModule::getType(std::string typeName, std::vector<BalanceType *> generics) {
    // Check if it is a type (or generic type which was already defined with generic types)
    for (auto const &x : types)
    {
        BalanceType * btype = x.second;
        if (btype->equalTo(this, typeName, generics)) {
            return btype;
        }
    }

    // Check if it is a generic type, and create if necessary
    for (auto const &x : genericTypes)
    {
        BalanceType * btype = x.second;
        if (btype->name == typeName && btype->generics.size() == generics.size()) {
            // TODO: Figure out where to register these functions
            BalanceType * newGenericType = nullptr;
            if (btype->name == "Array") {
                newGenericType = createType__Array(generics[0]);
            }

            if (newGenericType != nullptr && newGenericType->balanceModule != this) {
                createImportedClass(currentPackage->currentModule, newGenericType);
            }
            return newGenericType;
        }
    }

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

void BalanceModule::addType(BalanceType * balanceType) {
    int typeIndex = this->types.size();
    balanceType->typeIndex = typeIndex;
    this->types[balanceType->toString()] = balanceType;

    // Create typeInfo for type
    ArrayRef<Constant *> valuesRef({
        // typeId
        ConstantInt::get(*currentPackage->context, llvm::APInt(32, typeIndex, true)),
        // name
        currentPackage->currentModule->builder->CreateGlobalStringPtr(balanceType->toString())
    });

    Constant * typeInfoData = ConstantStruct::get(this->typeInfoStructType, valuesRef);
    balanceType->typeInfoVariable = typeInfoData;
    // new GlobalVariable(*currentPackage->currentModule->module, (StructType *) typeInfoType->getInternalType(), true, GlobalValue::ExternalLinkage, typeInfoData, balanceType->toString() + "_typeInfo");
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
