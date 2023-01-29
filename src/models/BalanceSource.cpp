#include "BalanceSource.h"
#include "../BalancePackage.h"

extern BalancePackage * currentPackage;

std::string BalanceSource::getString() {
    if (!this->filePath.empty()) {
        std::ifstream t(this->filePath);
        std::stringstream buffer;
        buffer << t.rdbuf();
        return buffer.str();
    }
    return this->programString;
}