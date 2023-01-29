#ifndef BALANCE_SOURCE_H
#define BALANCE_SOURCE_H

#include <string>

class BalanceSource
{
public:
    std::string filePath = "";
    std::string programString = "";

    std::string getString();
};

#endif