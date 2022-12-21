#include "TypeTests.h"

#include <iostream>
#include "TestHelpers.h"
#include "../src/BalancePackage.h"

void duplicateClassName() {
    std::string program = R""""(
class MyClass {}
class MyClass {}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Duplicate class, type already exist: MyClass", error->message, program);
}

// test duplicate parameter name in same class method signature
void duplicateParameterName() {
    std::string program = R""""(
class MyClass {
    test(Int a, Int a) {}
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Duplicate parameter name: a", error->message, program);
}

void runTypesTestSuite() {
    std::cout << "RUNNING TYPE TESTS" << std::endl;
    duplicateClassName();
    duplicateParameterName();
}