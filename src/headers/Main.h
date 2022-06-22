#ifndef MAIN_H
#define MAIN_H

#include "Visitor.h"

#include "llvm/IR/Module.h"
#include "antlr4-runtime.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeinfo>
#include <typeindex>

using namespace antlrcpptest;
using namespace llvm;
using namespace antlr4;

void initializeModule(BalanceModule * bmodule);

#endif