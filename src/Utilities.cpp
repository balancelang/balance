#include "headers/Utilities.h"

#include <iostream>
#include <map>
#include <cstdio>
#include <fstream>

bool file_exist(std::string fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}