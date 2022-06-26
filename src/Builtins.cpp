#include "headers/Builtins.h"
#include "headers/Package.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;

void create_function_print()
{
    llvm::FunctionType * functionType = llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*currentPackage->currentModule->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->currentModule->context), 0), true);
    llvm::FunctionCallee printfFunc = currentPackage->currentModule->module->getOrInsertFunction("printf", functionType);
}

void create_functions() {
    create_function_print();
}

void create_array_type() {
    // BalanceType * type = new BalanceType(module, , "Array");
    // type->
    // auto types = ArrayRef<Type *> { ArrayType };
    // StringRef typeName = "Array";
    // StructType::create(types, typeName, false);
}

void createIntegerToStringMethod() {
    // string functionName = "toString()"
    // // TODO: Implicit first argument is 'this' ? The integer
    // vector<Type *> functionParameterTypes;
    // vector<string> functionParameterNames;

    // Type *returnType = getBuiltinType("String");

    // ArrayRef<Type *> parametersReference(functionParameterTypes);
    // FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
    // Function *function = Function::Create(functionType, Function::InternalLinkage, functionName, module);

    // // Add parameter names
    // Function::arg_iterator args = function->arg_begin();
    // for (string parameterName : functionParameterNames)
    // {
    //     llvm::Value *x = args++;
    //     x->setName(parameterName);
    //     setValue(parameterName, x); // TODO: Shouldn't this happen after updating currentScope?
    // }

    // BasicBlock *functionBody = BasicBlock::Create(*context, functionName + "_body", function);
    // // Store current block so we can return to it after function declaration
    // BasicBlock *resumeBlock = builder->GetInsertBlock();
    // builder->SetInsertPoint(functionBody);
    // ScopeBlock *scope = currentScope;
    // currentScope = new ScopeBlock(functionBody, scope);

    // visit(ctx->functionBlock());

    // if (returnType->isVoidTy())
    // {
    //     builder->CreateRetVoid();
    // }

    // bool hasError = verifyFunction(*function);

    // builder->SetInsertPoint(resumeBlock);
    // type->addMethod("toString");
}

void createIntegerType() {
    // BalanceType * type = new BalanceType(module.get(), getBuiltinType("Int"), "Int");
    // string functionName = "toString";
    // vector<Type *> functionParameterTypes;
    // functionParameterTypes.push_back(getBuiltinType("Int"));
    // Type *returnType = getBuiltinType("String");
    // ArrayRef<Type *> parametersReference(functionParameterTypes);
    // FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);
    // Function *function = Function::Create(functionType, Function::InternalLinkage, functionName, module.get());

    // Function::arg_iterator args = function->arg_begin();
    // Value * thisParameter = args++;
    // thisParameter->setName("this");

    // BasicBlock *functionBody = BasicBlock::Create(*context, functionName + "_body", function);
    // // Store current block so we can return to it
    // BasicBlock *resumeBlock = builder->GetInsertBlock();
    // ScopeBlock *scope = currentScope;
    // builder->SetInsertPoint(functionBody);

    // builder->CreateRet(thisParameter);
    // builder->SetInsertPoint(resumeBlock);

    // FunctionType * functionType = FunctionType::get(getBuiltinType("String"), getBuiltinType("Int"), true);
    // FunctionCallee printfFunc = module->getOrInsertFunction("itos", functionType);
}

void create_types() {
    createIntegerType();
    create_array_type();
}
