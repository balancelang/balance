#include "File.h"
#include "../Builtins.h"
#include "../Package.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"

extern BalancePackage *currentPackage;
extern llvm::Value *accessedValue;

void createType__String() {
    // Define the string type as a { i32*, i32 } - pointer to the string and size of the string
    BalanceClass * bclass = new BalanceClass("String");
    currentPackage->currentModule->classes["String"] = bclass;
    bclass->properties["stringPointer"] = new BalanceProperty("stringPointer", "", 0);
    bclass->properties["stringPointer"]->type = llvm::Type::getInt8PtrTy(*currentPackage->context);
    bclass->properties["stringSize"] = new BalanceProperty("stringSize", "", 1);
    bclass->properties["stringSize"]->type = llvm::Type::getInt32Ty(*currentPackage->context);

    currentPackage->currentModule->currentClass = bclass;
    StructType *structType = StructType::create(*currentPackage->context, "String");
    ArrayRef<Type *> propertyTypesRef({
        // Pointer to the String
        llvm::Type::getInt8PtrTy(*currentPackage->context),
        // Size of the string
        llvm::Type::getInt32Ty(*currentPackage->context)
        // TODO: We could have an optional pointer to the next part of the string
    });
    structType->setBody(propertyTypesRef, false);
    bclass->structType = structType;
    bclass->hasBody = true;

    createDefaultConstructor(currentPackage->currentModule, bclass);

    currentPackage->currentModule->currentClass = nullptr;
}

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
    // Make sure there's a declaration for fwrite
    // size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
    ArrayRef<Type *> params({
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::Type::getInt32Ty(*currentPackage->context),
        llvm::Type::getInt32Ty(*currentPackage->context),
        llvm::Type::getInt32PtrTy(*currentPackage->context)
    });
    llvm::FunctionType * fwriteDeclarationType = llvm::FunctionType::get(llvm::Type::getInt32Ty(*currentPackage->context), params, false);
    currentPackage->currentModule->module->getOrInsertFunction("fwrite", fwriteDeclarationType);

    std::string functionName = "write";
    std::string functionNameWithClass = "File_" + functionName;
    BalanceClass * stringClass = currentPackage->builtins->getClass("String");

    ArrayRef<Type *> parametersReference({
        currentPackage->currentModule->currentClass->structType->getPointerTo(),  // this argument
        stringClass->structType->getPointerTo()
    });

    Type * returnType = llvm::Type::getVoidTy(*currentPackage->context);
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * writeFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", writeFunc);

    currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, {}, "Int");
    currentPackage->currentModule->currentClass->methods[functionName]->function = writeFunc;
    currentPackage->currentModule->currentClass->methods[functionName]->returnType = getBuiltinType("None");

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    int intIndex = currentPackage->currentModule->currentClass->properties["filePointer"]->index;
    Value * zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));

    Function::arg_iterator args = writeFunc->arg_begin();
    llvm::Value *thisPointer = args++;
    llvm::Value *stringPointer = args++;        // This is pointer to struct of type String

    Type * fileStructType = thisPointer->getType()->getPointerElementType();
    Value * ptr = currentPackage->currentModule->builder->CreateGEP(fileStructType, thisPointer, {zero, index});
    Value * filePtr  = currentPackage->currentModule->builder->CreateLoad(ptr);

    // Pull out string pointer and string size
    Value * stringPointerIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["stringPointer"]->index, true));
    Value * stringSizeIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["stringSize"]->index, true));
    Value * stringPointerValue = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringPointer, {zero, stringPointerIndex});
    Value * stringSizeValue = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringPointer, {zero, stringSizeIndex});
    Value * loadedPointerValue = currentPackage->currentModule->builder->CreateLoad(stringPointerValue);
    Value * loadedStringSizeValue = currentPackage->currentModule->builder->CreateLoad(stringSizeValue);

    // CreateCall to fwrite with filePtr as argument.
    Function * fwriteFunc = currentPackage->currentModule->module->getFunction("fwrite");
    ArrayRef<Value *> arguments({
        loadedPointerValue,                                                                 // const void *ptr      (string pointer)
        ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)),               // size_t size
        loadedStringSizeValue,              // size_t nmemb     TODO: currently hardcoded to 10
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