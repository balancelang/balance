#ifndef BALANCE_CLASS_H
#define BALANCE_CLASS_H

#include "BalanceProperty.h"
#include "BalanceFunction.h"
#include "BalanceModule.h"

#include "llvm/IR/DerivedTypes.h"

class BalanceImportedFunction;
class BalanceModule;
class BalanceFunction;
class BalanceProperty;
class BalanceImportedConstructor;


class BalanceClass
{
public:
    string name;
    map<string, BalanceProperty *> properties = {};
    map<string, BalanceFunction *> methods = {};
    llvm::Function *constructor = nullptr;
    llvm::StructType *structType = nullptr;
    bool hasBody;
    BalanceModule *module;
    BalanceClass(string name)
    {
        this->name = name;
    }

    bool finalized();
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

#endif