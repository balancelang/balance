#include "CompileTests.h"
#include "TestHelpers.h"

void createPrintInt() {
    std::string program = "var a = 555\nprint(a)";
    std::string result = run(program);
    assertEqual("555\n", result, program);
}

void createPrintBool() {
    std::string program = "var a = true\nprint(a)";
    std::string result = run(program);
    assertEqual("true\n", result, program);
}

void createPrintDouble() {
    std::string program = "var a = 5.5\nprint(a)";
    std::string result = run(program);
    assertEqual("5.5\n", result, program);
}

void createPrintString() {
    std::string program = "var a = \"HELLO\"\nprint(a)";
    std::string result = run(program);
    assertEqual("HELLO\n", result, program);
}

void createPrintArray() {
    std::string program = "var a = [1,2,3,4]\nprint(a)";
    std::string result = run(program);
    assertEqual("[1, 2, 3, 4]\n", result, program);
}

void createFunctionAndInvoke() {
    std::string program = "test(Int a): None {\nprint(a)\n}\ntest(55)";
    std::string result = run(program);
    assertEqual("55\n", result, program);
}

void createLambdaAndInvoke() {
    std::string program = "var lamb = (Int a): None -> {\nprint(a)\n}\nlamb(55)";
    std::string result = run(program);
    assertEqual("55\n", result, program);
}

void createReassignment() {
    std::string program = "var a = 555\na=222\nprint(a)";
    std::string result = run(program);
    assertEqual("222\n", result, program);
}

void operatorsTwoOperandsTest() {
    std::string program = "print(46 + 2)";
    std::string result = run(program);
    assertEqual("48\n", result, program);

    program = "print(46 - 2)";
    result = run(program);
    assertEqual("44\n", result, program);

    program = "print(8 * 2)";
    result = run(program);
    assertEqual("16\n", result, program);

    program = "print(8 / 2)";
    result = run(program);
    assertEqual("4\n", result, program);
}

void operatorsThreeOperandsTest() {
    std::string program = "print(46 + 2 - 2)";
    std::string result = run(program);
    assertEqual("46\n", result, program);

    program = "print(46 - 2 * 2)";
    result = run(program);
    assertEqual("42\n", result, program);

    program = "print(8 * 2 + 6)";
    result = run(program);
    assertEqual("22\n", result, program);

    program = "print(6 + 8 * 2)";
    result = run(program);
    assertEqual("22\n", result, program);

    program = "print(8 / 2)";
    result = run(program);
    assertEqual("4\n", result, program);
}


    // func with no arguments and no return
    // func with no arguments and return
    // func with 1 argument and no return
    // func with 1 argument and return
    // func with 2 arguments and return
    // ^-- same for lambda
    // func with lambda as parameter

    // 5.5 + 5 (cast int to float)
    // 5 + 5.5
    // 5.5 - 5
    // 5 - 5.5
    // 2.2 + true
    // 2.2 - true
    // 2.2 + false
    // 2.2 - false

void classTest_classFunction() {
    std::string program = R""""(
class ClassA {
    someFunction(Int a, Int b): Int {
        return a + b
    }
}

var clsA = new ClassA()
print(clsA.someFunction(2,3))
    )"""";
    std::string result = run(program);
    assertEqual("5\n", result, program);
}

void classTest_classProperty() {
    std::string program = R""""(
class ClassA {
    Int x
}

var clsA = new ClassA()
print(clsA.x)
clsA.x = 55
print(clsA.x)
    )"""";
    std::string result = run(program);
    assertEqual("0\n55\n", result, program);
}

void classTest_classProperty_Interface() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int a, Int b): Int
}

class ClassA implements MyInterface {
    someFunction(Int a, Int b): Int {
        print("Inside classA")
        return a + b
    }
}

class ClassB {
    MyInterface a
}

var clsA = new ClassA()
var clsB = new ClassB()
clsB.a = clsA
print(clsB.a.someFunction(2,3))
    )"""";
    std::string result = run(program);
    assertEqual("Inside classA\n5\n", result, program);
}

void functionTakesInterface() {
    std::string program = R""""(
interface MyInterface {
    someFunction(Int a, Int b): Int
}
class ClassA implements MyInterface {
    someFunction(Int a, Int b): Int {
        print("Inside classA")
        return a + b
    }
}
functionTakesInterface(MyInterface x): None {
    print(x.someFunction(2,3))
}
var clsA = new ClassA()
functionTakesInterface(clsA)
    )"""";
    std::string result = run(program);
    assertEqual("Inside classA\n5\n", result, program);
}

// Test returning interface
void functionReturnsInterface() {
    std::string program = R""""(
interface MyInterface {
    function(): None
}
class MyClass implements MyInterface {
    function(): None {
        print("Inside MyClass")
    }
}
returnsInterface(): MyInterface {
    return new MyClass()
}
var x = returnsInterface()
x.function()
    )"""";
    std::string result = run(program);
    assertEqual("Inside MyClass\n", result, program);
}

void objectShorthandInitializer() {
    std::string program = R""""(
interface MyInterface {
    someFunction(): None
}
class A {
    Int x
}
class B implements MyInterface {
    someFunction(): None {
        print("Inside B")
    }
}
class MyClass {
    A a
    MyInterface b
    Int c
    Bool d
    String e
}
MyClass myClass = {
    a: new A(),
    b: new B(),
    c: 15,
    d: false,
    e: "Hello world"
}
myClass.a = 55
print(myClass.a)
myClass.b.someFunction()
print(myClass.c)
print(myClass.d)
print(myClass.e)
    )"""";
    std::string result = run(program);
    assertEqual("55\nInside B\n15\nfalse\nHello world\n", result, program);
}

void runCompileTestSuite() {
    puts("RUNNING COMPILE TESTS");

    createPrintInt();
    createPrintBool();
    createPrintDouble();
    createPrintString();
    createPrintArray();
    createFunctionAndInvoke();
    createLambdaAndInvoke();
    createReassignment();
    operatorsTwoOperandsTest();
    operatorsThreeOperandsTest();

    classTest_classFunction();
    classTest_classProperty();
    classTest_classProperty_Interface();

    functionTakesInterface();
    functionReturnsInterface();

    // objectShorthandInitializer();
}
