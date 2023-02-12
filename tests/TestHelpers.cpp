#include "TestHelpers.h"
#include <array>
#include "../src/BalancePackage.h"

std::string executeExecutable(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string run(std::string program) {
    std::cout << "Program: " << program << std::endl;
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    return executeExecutable("./program");
}

string replaceCharacter(string input, char character, char replaceCharacter) {
    string output = input;
    replace(output.begin(), output.end(), character, replaceCharacter);
    return output;
}

void assertEqual(string expected, string actual, string program) {
    if (actual == expected) {
        cout << "OK" << endl;
    } else {
        cout << endl;

        cout << "FAIL | Program: " << program << endl;
        cout << "Expected: " << expected << endl;
        cout << "Actual: " << actual << endl;

        cout << endl;

        // For now, we exit early on first error
        exit(1);
    }
}

void assertEqual(bool expected, bool actual, string program) {
    if (actual != expected) {
        cout << endl;
        cout << "FAIL | Program: " << program << endl;
        cout << "Expected: " << expected << endl;
        cout << "Actual: " << actual << endl;
        cout << endl;
        exit(1);
    }
}