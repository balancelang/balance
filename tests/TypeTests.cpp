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

// test duplicate interface name
void duplicateInterfaceName() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int x): None
}
interface MyInterface {
    someFunction123(Int x, Int y): None
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Duplicate interface, type already exist: MyInterface", error->message, program);
}

// test duplicate parameter name in same class method signature
void duplicateClassParameterName() {
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

// test duplicate parameter name in same interface method signature
void duplicateInterfaceParameterName() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int x, Int x): None
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Duplicate parameter name: x", error->message, program);
}

// test duplicate interface function name
void duplicateInterfaceMethod() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int x): None
    someFunction(Int x, Int y): None
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Duplicate interface method name, method already exist: someFunction", error->message, program);
}

void duplicateClassProperty() {
    std::string program = R""""(
class X {
    Int x
    Int x
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Duplicate property name, property already exist: x", error->message, program);
}

// test class interface function name
void duplicateClassMethod() {
    std::string program = R""""(
class MyClass {
    someFunction(Int x): None {}
    someFunction(Int x, Int y): None {}
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Duplicate class method name, method already exist: someFunction", error->message, program);
}

// test missing function implementation
void missingInterfaceImplementation() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int x): None
}
class MyClass implements MyInterface {}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Missing interface implementation of function someFunction, required by interface MyInterface", error->message, program);
}

// test wrong return type
void wrongInterfaceReturnType() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int x): Int
}
class MyClass implements MyInterface {
    someFunction(Int x): None {}
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Wrong return-type of implemented function. Expected Int, found None", error->message, program);
}

// test wrong parameters
void wrongInterfaceParametersType() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int x): None
}
class MyClass implements MyInterface {
    someFunction(Int x, Bool y): None {}
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Extraneous parameter in implemented method, someFunction, found Bool", error->message, program);
}

// test interface as property type
void interfaceAsClassProperty() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int x): None
}
class MyClass implements MyInterface {
    someFunction(Int x): None {
        print("Inside MyClass")
    }
}
class OtherClass {
    MyInterface x
}
var myClass = new MyClass()
var otherClass = new OtherClass()
otherClass.x = myClass
otherClass.x.someFunction(2)
    )"""";
    std::string result = run(program);
    assertEqual("Inside MyClass\n", result, program);
}

// test interface as function parameter
void interfaceAsFunctionParameter() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int x): None
}
class MyClass implements MyInterface {
    someFunction(Int x): None {
        print("Inside MyClass")
    }
}
var myClass = new MyClass()
takesInterface(MyInterface x): None {
    x.someFunction(5)
}
takesInterface(myClass)
    )"""";
    std::string result = run(program);
    assertEqual("Inside MyClass\n", result, program);
}

// test interface as return type
void interfaceAsFunctionReturnType() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int x): None
}
class MyClass implements MyInterface {
    someFunction(Int x): None {
        print("Inside MyClass")
    }
}
returnsInterface(): MyInterface {
    return new MyClass()
}
var result = returnsInterface()
result.someFunction(5)
    )"""";
    std::string result = run(program);
    assertEqual("Inside MyClass\n", result, program);
}

void classAsClassPropertyDirectAccess() {
    std::string program = R""""(
class MyClass {
    OtherClass x
}
class OtherClass {
    Int y
}
var myClass = new MyClass()
var otherClass = new OtherClass()
otherClass.y = 55
myClass.x = otherClass
print(myClass.x.y)
    )"""";
    std::string result = run(program);
    assertEqual("55\n", result, program);
}

void classAsClassPropertyFunctionAccess() {
    std::string program = R""""(
class MyClass {
    OtherClass x
    getX(): OtherClass {
        return x
    }
    setX(OtherClass xx) {
        x = xx
    }
}
class OtherClass {
    Int y
    getY(): Int {
        return y
    }
    setY(Int yy): None {
        y = yy
    }
}
var myClass = new MyClass()
var otherClass = new OtherClass()
otherClass.setY(55)
myClass.setX(otherClass)
print(myClass.getX().getY())
    )"""";
    std::string result = run(program);
    assertEqual("55\n", result, program);
}

void testNestedClassProperties() {
    std::string program = R""""(
class A {
    B b
}
class B {
    C c
}
class C {
    D d
}
class D {
    Int x
}
var a = new A()
a.b = new B()
a.b.c = new C()
a.b.c.d = new D()
a.b.c.d.x = 55
print(a.b.c.d.x)
    )"""";
    std::string result = run(program);
    assertEqual("55\n", result, program);
}


void runTypesTestSuite() {
    std::cout << "RUNNING TYPE TESTS" << std::endl;
    duplicateClassName();
    duplicateInterfaceName();

    duplicateClassParameterName();
    duplicateInterfaceParameterName();

    duplicateClassMethod();
    duplicateInterfaceMethod();
    duplicateClassProperty();

    missingInterfaceImplementation();

    wrongInterfaceReturnType();
    wrongInterfaceParametersType();

    interfaceAsClassProperty();
    interfaceAsFunctionParameter();
    interfaceAsFunctionReturnType();

    classAsClassPropertyDirectAccess();
    classAsClassPropertyFunctionAccess();

    testNestedClassProperties();
}