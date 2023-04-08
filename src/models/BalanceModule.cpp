#include "BalanceModule.h"
#include "BalanceValue.h"
#include "../builtins/Array.h"
#include "../builtins/Lambda.h"
#include "../BalancePackage.h"

#include "llvm/IR/Value.h"
#include "ParserRuleContext.h"

using namespace antlr4;

extern BalancePackage *currentPackage;

void BalanceModule::initializeModule() {
    this->builder = new llvm::IRBuilder<>(*currentPackage->context);
    this->module = new llvm::Module(this->name, *currentPackage->context);

    // Initialize module root scope
    FunctionType *funcType = FunctionType::get(this->builder->getInt32Ty(), false);
    std::string rootFunctionName = this->name + "_main";
    Function *rootFunc = Function::Create(funcType, Function::ExternalLinkage, this->isEntrypoint ? "main" : rootFunctionName, this->module);
    BasicBlock *entry = BasicBlock::Create(*currentPackage->context, "entrypoint", rootFunc);
    this->builder->SetInsertPoint(entry);
    this->rootScope = new BalanceScopeBlock(entry, nullptr);
    this->currentScope = this->rootScope;
}

void BalanceModule::initializeTypeInfoStruct() {
    llvm::StructType *structType = llvm::StructType::create(*currentPackage->context, "TypeInfo");
    llvm::ArrayRef<llvm::Type *> propertyTypesRef({
        Type::getInt32Ty(*currentPackage->context),
        this->builder->CreateGlobalStringPtr("")->getType()
    });
    structType->setBody(propertyTypesRef, false);

    currentPackage->typeInfoStructType = structType;
}

void BalanceModule::generateAST() {
    std::string result = "";

    for (BalanceSource * bsource : this->sources) {
        result += "\n\n" + bsource->getString();
    }

    this->antlrStream = new antlr4::ANTLRInputStream(result);
    this->lexer = new BalanceLexer(this->antlrStream);
    this->tokenStream = new antlr4::CommonTokenStream(this->lexer);
    this->tokenStream->fill();
    this->parser = new BalanceParser(this->tokenStream);
    this->tree = this->parser->root();
}

BalanceType * BalanceModule::getType(std::string typeName) {
    std::vector<BalanceType *> v;
    return this->getType(typeName, v);
}

BalanceType * BalanceModule::createGenericType(std::string typeName, std::vector<BalanceType *> generics) {
    // Check if it is a generic type, and create if necessary
    for (auto const &x : genericTypes)
    {
        BalanceType * btype = x.second;
        if (btype->name == typeName) {
            // TODO: Figure out where to register these functions
            BalanceType * newGenericType = nullptr;
            if (btype->name == "Array") {
                ArrayBalanceType * arrayType = (ArrayBalanceType *) currentPackage->getBuiltinType("Array");
                newGenericType = arrayType->registerGenericType(generics[0]);
            } else if (btype->name == "Lambda") {
                LambdaBalanceType * lambdaType = (LambdaBalanceType *) currentPackage->getBuiltinType("Lambda");
                newGenericType = lambdaType->registerGenericType(generics);
            }

            if (newGenericType != nullptr && newGenericType->balanceModule != this) {
                createImportedClass(currentPackage->currentModule, newGenericType);
            }

            if (newGenericType != nullptr) {
                currentPackage->currentModule->addType(newGenericType);
                return newGenericType;
            }
        }
    }

    // TODO: Make this a full string of X<Y> rather than just X
    throw std::runtime_error("Failed to create generic type: " + typeName);
}

BalanceType * BalanceModule::getType(std::string typeName, std::vector<BalanceType *> generics) {
    // Check if it is a type (or generic type which was already defined with generic types)
    for (BalanceType * btype : types)
    {
        if (btype->equalTo(typeName, generics)) {
            return btype;
        }
    }

    for (BalanceType * btype : importedTypes) {
        if (btype->equalTo(typeName, generics)) {
            return btype;
        }
    }

    // TODO: remove
    // for (auto const &x : genericTypes)
    // {
    //     BalanceType * btype = x.second;
    //     if (btype->name == typeName) {
    //         // TODO: Figure out where to register these functions
    //         BalanceType * newGenericType = nullptr;
    //         if (btype->name == "Array") {
    //             newGenericType = createType__Array(generics[0]);
    //         }

    //         if (newGenericType != nullptr && newGenericType->balanceModule != this) {
    //             createImportedClass(currentPackage->currentModule, newGenericType);
    //         }

    //         if (newGenericType != nullptr) {
    //             currentPackage->currentModule->addType(newGenericType);
    //             return newGenericType;
    //         }
    //     }
    // }

    return nullptr;
}

std::vector<BalanceType *> BalanceModule::getGenericVariants(std::string typeName) {
    std::vector<BalanceType *> types;

    BalanceType * baseType = this->genericTypes[typeName]; // TODO: Handle null
    for (BalanceType * btype : this->types) {
        if (btype->name == baseType->name) {
            types.push_back(btype);
        }
    }

    return types;
}

bool functionEqualTo(BalanceFunction * bfunction, std::string functionName, std::vector<BalanceType *> parameters) {
    if (bfunction->name != functionName) {
        return false;
    }

    if (bfunction->parameters.size() != parameters.size()) {
        return false;
    }

    for (int i = 0; i < bfunction->parameters.size(); i++) {
        if (!canAssignTo(nullptr, parameters[i], bfunction->parameters[i]->balanceType)) {
            return false;
        }
    }

    return true;
}

std::vector<BalanceFunction *> BalanceModule::getFunctionsByName(std::string functionName) {
    std::vector<BalanceFunction *> result;

    for (BalanceFunction * bfunction : functions)
    {
        if (bfunction->name == functionName) {
            result.push_back(bfunction);
        }
    }

    for (BalanceFunction * bfunction : importedFunctions)
    {
        if (bfunction->name == functionName) {
            result.push_back(bfunction);
        }
    }

    return result;
}

BalanceFunction * BalanceModule::getFunction(std::string functionName, std::vector<BalanceType *> parameters) {
    // TODO: Check for ambigiouity on overloaded functions
    // e.g. two functions where one takes base class and one takes interface, both implemented by X.
    for (BalanceFunction * bfunction : functions)
    {
        if (functionEqualTo(bfunction, functionName, parameters)) {
            return bfunction;
        }
    }

    for (BalanceFunction * bfunction : importedFunctions)
    {
        if (functionEqualTo(bfunction, functionName, parameters)) {
            return bfunction;
        }
    }

    return nullptr;
}

BalanceValue *BalanceModule::getValue(std::string variableName) {
    BalanceScopeBlock *scope = this->currentScope;
    while (scope != nullptr) {
        // Check variables etc
        if (scope->symbolTable.find(variableName) != scope->symbolTable.end()) {
            return scope->symbolTable[variableName];
        }

        scope = scope->parent;
    }

    return nullptr;
}

void BalanceModule::setValue(std::string variableName, BalanceValue *bvalue) {
    this->currentScope->symbolTable[variableName] = bvalue;
}

void BalanceModule::addType(BalanceType * balanceType) {
    // TODO: Check if type is already added, make sure we dont end up in a loop here
    // if (this->getType(balanceType->name, balanceType->generics) != nullptr) {
    //     return;
    // }

    // if (balanceType->isInternalType) {
    //     return;
    // }

    this->types.push_back(balanceType);


    // int typeIndex = this->types.size();
    // balanceType->typeIndex = typeIndex;

    // // Create typeInfo for type
    // ArrayRef<Constant *> valuesRef({
    //     // typeId
    //     ConstantInt::get(*currentPackage->context, llvm::APInt(32, typeIndex, true)),
    //     // name
    //     currentPackage->currentModule->builder->CreateGlobalStringPtr(balanceType->toString())
    // });

    // Constant * typeInfoData = ConstantStruct::get(currentPackage->typeInfoStructType, valuesRef);
    // balanceType->typeInfoVariable = typeInfoData;
}

void BalanceModule::addFunction(BalanceFunction * bfunction) {
    functions.push_back(bfunction);
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

bool BalanceModule::isTypeImported(BalanceType * btype) {
    for (BalanceType * importedType : this->importedTypes) {
        if (btype == importedType) {
            return true;
        }
    }
    return false;
}