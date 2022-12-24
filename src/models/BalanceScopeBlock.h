#ifndef BALANCE_SCOPE_BLOCK_H
#define BALANCE_SCOPE_BLOCK_H

#include "BalanceValue.h"

#include "llvm/IR/BasicBlock.h"
#include <map>

class BalanceValue;

class BalanceScopeBlock
{
public:
    llvm::BasicBlock *block;
    BalanceScopeBlock *parent;
    std::map<std::string, BalanceValue *> symbolTable;

    bool isTerminated = false;

    BalanceScopeBlock(llvm::BasicBlock *block, BalanceScopeBlock *parent)
    {
        this->block = block;
        this->parent = parent;
    }
};

#endif