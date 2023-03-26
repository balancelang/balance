#include "Int64.h"

#include "../../BalancePackage.h"
#include "../../models/BalanceType.h"

extern BalancePackage *currentPackage;

void createType__Int64() {
    BalanceType * int64Type = new BalanceType(currentPackage->currentModule, "Int64", llvm::Type::getInt64Ty(*currentPackage->context));
    int64Type->isSimpleType = true;
    currentPackage->currentModule->addType(int64Type);
}

void createFunctions__Int64() {

}