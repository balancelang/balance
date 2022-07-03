#include "File.h"
#include "../Builtins.h"
#include "../Package.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;
extern llvm::Value *accessedValue;

void createMethod_close() {
    std::string functionName = "close";
    std::string functionNameWithClass = "File_" + functionName;

    ArrayRef<Type *> parametersReference({
        currentPackage->currentModule->currentClass->structType->getPointerTo()  // this argument
    });

    Type * returnType = llvm::Type::getVoidTy(*currentPackage->context);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * closeFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", closeFunc);

    currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, {}, "Int");
    currentPackage->currentModule->currentClass->methods[functionName]->function = closeFunc;
    currentPackage->currentModule->currentClass->methods[functionName]->returnType = getBuiltinType("None");

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    int intIndex = currentPackage->currentModule->currentClass->properties["filePointer"]->index;
    Value * zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));

    Function::arg_iterator args = closeFunc->arg_begin();
    llvm::Value *thisPointer = args++;

    Type *structType = thisPointer->getType()->getPointerElementType();
    Value * ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisPointer, {zero, index});
    Value * filePtr  = currentPackage->currentModule->builder->CreateLoad(ptr);

    // Make sure there's a declaration for fclose (int fclose(FILE *stream))
    llvm::FunctionType * fcloseDeclarationType = llvm::FunctionType::get(llvm::Type::getInt32Ty(*currentPackage->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), false);
    currentPackage->currentModule->module->getOrInsertFunction("fclose", fcloseDeclarationType);

    // CreateCall to fclose with filePtr as argument.
    Function * fcloseFunc = currentPackage->currentModule->module->getFunction("fclose");
    ArrayRef<Value *> arguments({
        filePtr
    });
    currentPackage->currentModule->builder->CreateCall(fcloseFunc, arguments);
    currentPackage->currentModule->builder->CreateRetVoid();

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createMethod_write() {
    std::string functionName = "write";
    std::string functionNameWithClass = "File_" + functionName;

    ArrayRef<Type *> parametersReference({
        currentPackage->currentModule->currentClass->structType->getPointerTo(),  // this argument
        getBuiltinType("String")
    });

    Type * returnType = llvm::Type::getVoidTy(*currentPackage->context);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * closeFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", closeFunc);

    currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, {}, "Int");
    currentPackage->currentModule->currentClass->methods[functionName]->function = closeFunc;
    currentPackage->currentModule->currentClass->methods[functionName]->returnType = getBuiltinType("None");

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    int intIndex = currentPackage->currentModule->currentClass->properties["filePointer"]->index;
    Value * zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));

    Function::arg_iterator args = closeFunc->arg_begin();
    llvm::Value *thisPointer = args++;
    llvm::Value *stringPointer = args++;

    Type *structType = thisPointer->getType()->getPointerElementType();
    Value * ptr = currentPackage->currentModule->builder->CreateGEP(structType, thisPointer, {zero, index});
    Value * filePtr  = currentPackage->currentModule->builder->CreateLoad(ptr);

    // Make sure there's a declaration for fwrite
    // size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
    llvm::FunctionType * fwriteDeclarationType = llvm::FunctionType::get(llvm::Type::getInt32Ty(*currentPackage->context), llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0), false);
    currentPackage->currentModule->module->getOrInsertFunction("fwrite", fwriteDeclarationType);

    // CreateCall to fwrite with filePtr as argument.
    Function * fwriteFunc = currentPackage->currentModule->module->getFunction("fwrite");
    ArrayRef<Value *> arguments({
        stringPointer,                                                                      // const void *ptr      (string pointer)
        ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)),               // size_t size
        ConstantInt::get(*currentPackage->context, llvm::APInt(32, 10, true)),              // size_t nmemb     TODO: currently hardcoded to 10
        filePtr                                                                             // FILE *stream
    });
    currentPackage->currentModule->builder->CreateCall(fwriteFunc, arguments);
    currentPackage->currentModule->builder->CreateRetVoid();

    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);
}

void createType__File() {
    BalanceClass * bclass = new BalanceClass("File");
    currentPackage->currentModule->classes["File"] = bclass;
    bclass->properties["filePointer"] = new BalanceProperty("filePointer", "", 0);
    bclass->properties["filePointer"]->type = llvm::PointerType::get(llvm::Type::getInt32Ty(*currentPackage->context), 0);

    currentPackage->currentModule->currentClass = bclass;
    StructType *structType = StructType::create(*currentPackage->context, "File");
    ArrayRef<Type *> propertyTypesRef({
        // Pointer to the file
        llvm::Type::getInt32PtrTy(*currentPackage->context)
    });
    structType->setBody(propertyTypesRef, false);
    bclass->structType = structType;
    bclass->hasBody = true;

    createDefaultConstructor(currentPackage->currentModule, bclass);

    // Create close method
    createMethod_close();

    // Create write method
    createMethod_write();

    currentPackage->currentModule->currentClass = nullptr;
}