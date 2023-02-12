#ifndef BALANCE_TYPE_H
#define BALANCE_TYPE_H

#include <map>
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constant.h"

#include "BalanceProperty.h"
#include "BalanceFunction.h"
#include "BalanceModule.h"

class BalanceFunction;
class BalanceProperty;
class BalanceModule;

class BalanceType
{
public:
    std::string name;                                   // E.g. "Array" in "Array<Int>"
    std::vector<BalanceType *> generics;                // E.g. ["String", "Int"] in Dictionary<String, Int>
    std::map<std::string, BalanceType *> genericsMapping = {}; // E.g. [T => Int, T2 => Bool] if generics
    llvm::Type * internalType = nullptr;
    int typeIndex = 0;
    bool isPublic = true;                               // TODO: Not used yet, will determine if type can be referenced from Balance-code
    bool isSimpleType = false;
    bool isInterface = false;
    bool isGenericPlaceholder = false;
    BalanceModule *balanceModule;
    bool hasBody = false;
    llvm::Constant * typeInfoVariable = nullptr;

    // Used to store [Int, Double] if "T where T is Int | Double" (only support 'is' which means same or descendant type)
    std::vector<BalanceType *> typeRequirements = {};

    // TODO: We may introduce multiple inheritance one day
    std::vector<BalanceType *> parents = {};

    std::map<std::string, BalanceProperty *> properties = {};
    std::map<std::string, BalanceFunction *> methods = {};
    std::vector<BalanceFunction *> constructors = {};
    std::map<std::string, BalanceType *> interfaces = {};

    llvm::Function * initializer = nullptr;

    // If interface, this holds the vtable struct (the vtable function types)
    llvm::StructType *vTableStructType = nullptr;

    // If class, this holds each implemented vtable
    std::map<std::string, llvm::Value *> interfaceVTables = {};

    BalanceType(BalanceModule * balanceModule, std::string name, std::vector<BalanceType *> generics = {}) {
        this->balanceModule = balanceModule;
        this->name = name;
        this->generics = generics;
    }

    BalanceType(BalanceModule * balanceModule, std::string name, llvm::Type * internalType) {
        this->balanceModule = balanceModule;
        this->name = name;
        this->internalType = internalType;
    }

    void addParent(BalanceType * parentType);
    void addMethod(std::string name, BalanceFunction * method);
    void addConstructor(std::string name, std::vector<BalanceParameter *> parameters);
    std::vector<BalanceFunction *> getMethods();
    BalanceFunction * getMethod(std::string key);
    BalanceFunction * getConstructor(std::vector<BalanceType *> parameters);
    int getMethodIndex(std::string key);
    llvm::Type * getReferencableType();
    llvm::Type * getInternalType();
    bool isFloatingPointType();
    std::string toString();
    bool equalTo(std::string name, std::vector<BalanceType *> generics);
    bool equalTo(BalanceType * other);
    llvm::Function * getInitializer();
    BalanceProperty * getProperty(std::string propertyName);
    std::vector<BalanceProperty *> getProperties();
    bool finalized();
    std::vector<BalanceType *> getHierarchy();
};

#endif