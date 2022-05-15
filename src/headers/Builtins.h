#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"

using namespace llvm;
using namespace std;

// Types

class BalanceType {
public:
    Module * module;
    Type * type;
    string name;
    BalanceType(Module * module, Type * type, string name) {
        this->module = module;
        this->type = type;
        this->name = name;
    }
    void addMethod(string name, FunctionType type);
    void addProperty(string name);
};



// Functions
void create_function_print();

// All
void create_functions();
void create_types();