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

void createMethod_read() {
    // Make sure there's a declaration for fread
    // size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
    ArrayRef<Type *> freadParams({
        llvm::PointerType::get(llvm::Type::getInt8Ty(*currentPackage->context), 0),
        llvm::Type::getInt64Ty(*currentPackage->context),
        llvm::Type::getInt64Ty(*currentPackage->context),
        llvm::Type::getInt32PtrTy(*currentPackage->context)
    });
    llvm::FunctionType * freadDeclarationType = llvm::FunctionType::get(llvm::Type::getInt32Ty(*currentPackage->context), freadParams, false);
    currentPackage->currentModule->module->getOrInsertFunction("fread", freadDeclarationType);

    // fseek
    // int fseek(FILE *stream, long int offset, int whence)
    ArrayRef<Type *> fseekParams({
        llvm::Type::getInt32PtrTy(*currentPackage->context),
        llvm::Type::getInt64Ty(*currentPackage->context),
        llvm::Type::getInt32Ty(*currentPackage->context)
    });
    llvm::FunctionType * fseekDeclarationType = llvm::FunctionType::get(llvm::Type::getInt32Ty(*currentPackage->context), fseekParams, false);
    currentPackage->currentModule->module->getOrInsertFunction("fseek", fseekDeclarationType);

    // ftell  TODO: This should be 64bit
    // long int ftell(FILE *stream)
    ArrayRef<Type *> ftellParams({
        llvm::Type::getInt32PtrTy(*currentPackage->context)
    });
    llvm::FunctionType * ftellDeclarationType = llvm::FunctionType::get(llvm::Type::getInt64Ty(*currentPackage->context), ftellParams, false);
    currentPackage->currentModule->module->getOrInsertFunction("ftell", ftellDeclarationType);

    // malloc
    // void *malloc(size_t size)
    // ArrayRef<Type *> mallocParams({
    //     llvm::Type::getInt64Ty(*currentPackage->context)
    // });
    // llvm::FunctionType * mallocDeclarationType = llvm::FunctionType::get(llvm::Type::getInt8PtrTy(*currentPackage->context), mallocParams, false);
    // currentPackage->currentModule->module->getOrInsertFunction("malloc", mallocDeclarationType);

    std::string functionName = "read";
    std::string functionNameWithClass = "File_" + functionName;
    BalanceClass * stringClass = currentPackage->builtins->getClass("String");

    ArrayRef<Type *> parametersReference({
        currentPackage->currentModule->currentClass->structType->getPointerTo()  // File "this" argument
    });

    Type * returnType = stringClass->structType->getPointerTo();
    FunctionType *functionType = FunctionType::get(returnType, parametersReference, false);

    llvm::Function * closeFunc = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);
    BasicBlock *functionBody = BasicBlock::Create(*currentPackage->context, functionName + "_body", closeFunc);

    currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, {}, "String");
    currentPackage->currentModule->currentClass->methods[functionName]->function = closeFunc;
    currentPackage->currentModule->currentClass->methods[functionName]->returnType = returnType;

    // Store current block so we can return to it after function declaration
    BasicBlock *resumeBlock = currentPackage->currentModule->builder->GetInsertBlock();
    currentPackage->currentModule->builder->SetInsertPoint(functionBody);

    Function::arg_iterator args = closeFunc->arg_begin();
    llvm::Value *thisPointer = args++;

    int intIndex = currentPackage->currentModule->currentClass->properties["filePointer"]->index;
    Value * zero = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    Value * index = ConstantInt::get(*currentPackage->context, llvm::APInt(32, intIndex, true));

    Type * fileStructType = thisPointer->getType()->getPointerElementType();
    Value * ptr = currentPackage->currentModule->builder->CreateGEP(fileStructType, thisPointer, {zero, index});
    Value * filePtr  = currentPackage->currentModule->builder->CreateLoad(ptr);

    // fseek(f, 0, SEEK_END);
    Function * fseekFunc = currentPackage->currentModule->module->getFunction("fseek");
    ArrayRef<Value *> fseekEndArguments({
        filePtr,
        ConstantInt::get(*currentPackage->context, llvm::APInt(64, 0, true)),
        ConstantInt::get(*currentPackage->context, llvm::APInt(32, 2, true)), // SEEK_END = 2
    });
    currentPackage->currentModule->builder->CreateCall(fseekFunc, fseekEndArguments);

    // long fsize = ftell(f);
    Function * ftellFunc = currentPackage->currentModule->module->getFunction("ftell");
    ArrayRef<Value *> ftellArguments({
        filePtr
    });

    Value * fileSizeValue = currentPackage->currentModule->builder->CreateCall(ftellFunc, ftellArguments);

    // fseek(f, 0, SEEK_SET);
    ArrayRef<Value *> fseekSetArguments({
        filePtr,
        ConstantInt::get(*currentPackage->context, llvm::APInt(64, 0, true)),
        ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true)), // SEEK_SET = 0
    });
    currentPackage->currentModule->builder->CreateCall(fseekFunc, fseekSetArguments);

    // Add 1 for null-terminated string
    Value * fileSizePlusOneValue = currentPackage->currentModule->builder->CreateAdd(fileSizeValue, ConstantInt::get(*currentPackage->context, llvm::APInt(64, 1, true)));

    auto memoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),   // input type?
        llvm::Type::getInt8Ty(*currentPackage->context),    // output type, which we get pointer to?
        fileSizePlusOneValue,                               // size, matches input type?
        nullptr,
        nullptr,
        ""
    );
    currentPackage->currentModule->builder->Insert(memoryPointer);

    // fread(string, fsize, 1, f);
    ArrayRef<Value *> freadArguments({
        memoryPointer,
        fileSizeValue,
        ConstantInt::get(*currentPackage->context, llvm::APInt(64, 1, true)),
        filePtr
    });
    Function * freadFunc = currentPackage->currentModule->module->getFunction("fread");
    currentPackage->currentModule->builder->CreateCall(freadFunc, freadArguments);

    // null-terminate string
    auto nullGEPValue = currentPackage->currentModule->builder->CreateGEP(memoryPointer, {fileSizeValue});
    auto zeroConstant = ConstantInt::get(*currentPackage->context, llvm::APInt(8, 0, true));
    currentPackage->currentModule->builder->CreateStore(zeroConstant, nullGEPValue);

    // Create string and set pointer + size
    auto stringMemoryPointer = llvm::CallInst::CreateMalloc(
        currentPackage->currentModule->builder->GetInsertBlock(),
        llvm::Type::getInt64Ty(*currentPackage->context),           // input type?
        stringClass->structType,                                    // output type, which we get pointer to?
        ConstantExpr::getSizeOf(stringClass->structType),           // size, matches input type?
        nullptr,
        nullptr,
        ""
    );
    currentPackage->currentModule->builder->Insert(stringMemoryPointer);

    int pointerIndex = stringClass->properties["stringPointer"]->index;
    auto pointerZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto pointerIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, pointerIndex, true));
    auto pointerGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringMemoryPointer, {pointerZeroValue, pointerIndexValue});

    currentPackage->currentModule->builder->CreateStore(memoryPointer, pointerGEP);

    int sizeIndex = stringClass->properties["length"]->index;
    auto sizeZeroValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, 0, true));
    auto sizeIndexValue = ConstantInt::get(*currentPackage->context, llvm::APInt(32, sizeIndex, true));
    auto sizeGEP = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringMemoryPointer, {sizeZeroValue, sizeIndexValue});

    Value * bitcastedFileSizeValue = currentPackage->currentModule->builder->CreateIntCast(fileSizeValue, llvm::Type::getInt32Ty(*currentPackage->context), false);
    currentPackage->currentModule->builder->CreateStore(bitcastedFileSizeValue, sizeGEP);

    currentPackage->currentModule->builder->CreateRet(stringMemoryPointer);
    currentPackage->currentModule->builder->SetInsertPoint(resumeBlock);

    bool hasError = verifyFunction(*closeFunc, &llvm::errs());
    if (hasError) {
        currentPackage->currentModule->module->print(llvm::errs(), nullptr);
        // exit(1);
    }
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

    currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, {}, "None");
    currentPackage->currentModule->currentClass->methods[functionName]->function = writeFunc;
    currentPackage->currentModule->currentClass->methods[functionName]->returnType = returnType;

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
    Value * stringLengthIndex = ConstantInt::get(*currentPackage->context, llvm::APInt(32, stringClass->properties["length"]->index, true));
    Value * stringPointerValue = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringPointer, {zero, stringPointerIndex});
    Value * stringLengthValue = currentPackage->currentModule->builder->CreateGEP(stringClass->structType, stringPointer, {zero, stringLengthIndex});
    Value * loadedPointerValue = currentPackage->currentModule->builder->CreateLoad(stringPointerValue);
    Value * loadedStringLengthValue = currentPackage->currentModule->builder->CreateLoad(stringLengthValue);

    // CreateCall to fwrite with filePtr as argument.
    Function * fwriteFunc = currentPackage->currentModule->module->getFunction("fwrite");
    ArrayRef<Value *> arguments({
        loadedPointerValue,                                                                 // const void *ptr      (string pointer)
        ConstantInt::get(*currentPackage->context, llvm::APInt(32, 1, true)),               // size_t size
        loadedStringLengthValue,              // size_t nmemb     TODO: currently hardcoded to 10
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

    // Create read method
    createMethod_read();

    currentPackage->currentModule->currentClass = nullptr;
}