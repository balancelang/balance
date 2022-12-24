#include <iostream>

std::string executeExecutable(const char* cmd);
std::string run(std::string program);
std::string replaceCharacter(std::string input, char character, char replaceCharacter);
void assertEqual(std::string expected, std::string actual, std::string program);
void assertEqual(bool expected, bool actual, std::string program);