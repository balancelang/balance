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

Module *buildModuleFromStream(ANTLRInputStream stream);
Module *buildModuleFromString(std::string program);
Module *buildModuleFromPath(std::string path);
tree::ParseTree *buildASTFromString(std::string program);