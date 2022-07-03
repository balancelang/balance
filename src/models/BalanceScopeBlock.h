#ifndef BALANCE_SCOPE_BLOCK_H
#define BALANCE_SCOPE_BLOCK_H

#include "llvm/IR/BasicBlock.h"
#include <map>

class BalanceScopeBlock
{
public:
    llvm::BasicBlock *block;
    BalanceScopeBlock *parent;
    std::map<std::string, llvm::Value *> symbolTable;

    BalanceScopeBlock(llvm::BasicBlock *block, BalanceScopeBlock *parent)
    {
        this->block = block;
        this->parent = parent;
    }
};

#endif