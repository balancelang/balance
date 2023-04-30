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
    someFunction(x: Int): None
}
interface MyInterface {
    someFunction123(x: Int, y: Int): None
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
    test(a: Int, a: Int) {}
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
    someFunction(x: Int, x: Int): None
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Duplicate parameter name: x", error->message, program);
}

void duplicateClassProperty() {
    std::string program = R""""(
class X {
    x: Int
    x: Int
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Duplicate property name, property already exist: x", error->message, program);
}

// test missing function implementation
void missingInterfaceImplementation() {
    std::string program = R""""(
interface MyInterface {
    someFunction(x: Int): None
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
    someFunction(x: Int): Int
}
class MyClass implements MyInterface {
    someFunction(x: Int): None {}
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
    someFunction(x: Int): None
}
class MyClass implements MyInterface {
    someFunction(x: Int, y: Bool): None {}
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Missing interface implementation of function someFunction, required by interface MyInterface", error->message, program);
}

// test interface as property type
void interfaceAsClassProperty() {
    std::string program = R""""(
interface MyInterface {
    someFunction(x: Int): None
}
class MyClass implements MyInterface {
    someFunction(x: Int): None {
        print("Inside MyClass")
    }
}
class OtherClass {
    x: MyInterface
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
    someFunction(x: Int): None
}
class MyClass implements MyInterface {
    someFunction(x: Int): None {
        print("Inside MyClass")
    }
}
var myClass = new MyClass()
takesInterface(x: MyInterface): None {
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
    someFunction(x: Int): None
}
class MyClass implements MyInterface {
    someFunction(x: Int): None {
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
    x: OtherClass
}
class OtherClass {
    y: Int
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
    x: OtherClass
    getX(): OtherClass {
        return x
    }
    setX(xx: OtherClass) {
        x = xx
    }
}
class OtherClass {
    y: Int
    getY(): Int {
        return y
    }
    setY(yy: Int): None {
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
    b: B
}
class B {
    c: C
}
class C {
    d: D
}
class D {
    x: Int
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

void testLhsEqualToRhs_ints() {
    std::string program = R""""(
var x: Int = 5.5
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Double cannot be assigned to Int", error->message, program);
}

void testLhsEqualToRhs_intsReassignment() {
    std::string program = R""""(
var x: Int = 5
x = 5.5
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Double cannot be assigned to Int", error->message, program);
}

void testLhsEqualToRhs_lambda() {
    std::string program = R""""(
var x: Lambda<Int, None> = (x: Int, y: Int): None -> {
    print("Hello")
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Lambda<Int, Int, None> cannot be assigned to Lambda<Int, None>", error->message, program);
}

void testLhsEqualToRhs_lambdaReassignment() {
    std::string program = R""""(
var x: Lambda<Int, None> = (x: Int): None -> {
    print("Hello")
}
x = (x: Int, y: Int): None -> {
    print("Hello again")
}
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Lambda<Int, Int, None> cannot be assigned to Lambda<Int, None>", error->message, program);
}


// test shorthand: duplicate property
void testShortHandInititalizerDuplicateProperty() {
        std::string program = R""""(
class MyClass {
    a: Int
}
var myClass: MyClass = { a: 123, a: 456 }
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Duplicate property in initializer: a", error->message, program);
}

// test shorthand: unknown property
void testShortHandInititalizerUnknownProperty() {
        std::string program = R""""(
class MyClass {
    a: Int
}
var myClass: MyClass = { a: 123, xyz: 456 }
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Unknown property in initializer: xyz", error->message, program);
}

// test shorthand: missing property
void testShortHandInititalizerMissingProperty() {
        std::string program = R""""(
class MyClass {
    a: Int
    b: Int
}
var myClass: MyClass = { a: 123 }
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Missing property in initializer: b", error->message, program);
}

// test shorthand: inheritance example
void testShortHandInititalizerInheritance() {
        std::string program = R""""(
class Parent {
    b: Int
}
class MyClass extends Parent {
    a: Int
}
var myClass: MyClass = { a: 123 }
    )"""";
    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(program);
    assertEqual(false, success, program);
    TypeError * error = package->modules["program"]->typeErrors[0];
    assertEqual("Missing property in initializer: b", error->message, program);
}

void runTypesTestSuite() {
    std::cout << "RUNNING TYPE TESTS" << std::endl;
    duplicateClassName();
    duplicateInterfaceName();

    duplicateClassParameterName();
    duplicateInterfaceParameterName();

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

    testLhsEqualToRhs_ints();
    testLhsEqualToRhs_intsReassignment();
    testLhsEqualToRhs_lambda();
    testLhsEqualToRhs_lambdaReassignment();

    testShortHandInititalizerDuplicateProperty();
    testShortHandInititalizerUnknownProperty();
    testShortHandInititalizerMissingProperty();
    testShortHandInititalizerInheritance();
}