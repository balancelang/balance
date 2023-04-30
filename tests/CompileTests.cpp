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
    std::string program = "test(a: Int): None {\nprint(a)\n}\ntest(55)";
    std::string result = run(program);
    assertEqual("55\n", result, program);
}

void createLambdaAndInvoke() {
    std::string program = "var lamb = (a: Int): None -> {\nprint(a)\n}\nlamb(55)";
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
    someFunction(a: Int, b: Int): Int {
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
    x: Int
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
    someFunction(a: Int, b: Int): Int
}

class ClassA implements MyInterface {
    someFunction(a: Int, b: Int): Int {
        print("Inside classA")
        return a + b
    }
}

class ClassB {
    a: MyInterface
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
    someFunction(a: Int, b: Int): Int
}
class ClassA implements MyInterface {
    someFunction(a: Int, b: Int): Int {
        print("Inside classA")
        return a + b
    }
}
functionTakesInterface(x: MyInterface): None {
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
    x: Int
}
class B implements MyInterface {
    someFunction(): None {
        print("Inside B")
    }
}
class MyClass {
    a: A
    b: MyInterface
    c: Int
    d: Bool
    e: String
}
var myClass: MyClass = {
    a: new A(),
    b: new B(),
    c: 15,
    d: false,
    e: "Hello world"
}
print(myClass.a.x)
myClass.b.someFunction()
print(myClass.c)
print(myClass.d)
print(myClass.e)
    )"""";
    std::string result = run(program);
    assertEqual("0\nInside B\n15\nfalse\nHello world\n", result, program);
}

void inheritedMembers() {
    std::string program = R""""(
class A {
    x: Int
    y: Int
    getX(): Int {
        return x
    }
}
class B extends A {
    z: Int
    getY(): Int {
        return y
    }
    getZ(): Int {
        return z
    }
}
var b = new B()
b.x = 1
b.y = 2
b.z = 3
print(b.getX())
print(b.getY())
print(b.getZ())
    )"""";
    std::string result = run(program);
    assertEqual("1\n2\n3\n", result, program);
}

void functionTakesBaseClass() {
    std::string program = R""""(
class A {
    x: Int
}
class B extends A {
    y: Int
}
var b = new B()
b.x = 1
b.y = 2
takesBase(a: A): None {
    print(a.x)
}
takesBase(b)
    )"""";
    std::string result = run(program);
    assertEqual("1\n", result, program);
}

void functionTakesAny() {
    std::string program = R""""(
class A {
    x: Int
}
var a = new A()
a.x = 1
takesAny(x: Any): None {
    print(x.getType().name)
}
takesAny(a)
takesAny("abc")
    )"""";
    std::string result = run(program);
    assertEqual("A\nString\n", result, program);
}

void functionOverloading() {
    std::string program = R""""(
function1(): None {
    print("function1 with no parameters")
}

function1(a: Int): None {
    print("function1 with Int parameter")
}

function1(a: Int, b: Int): None {
    print("function1 with two Int parameters")
}

function1()
function1(5)
function1(5, 5)
    )"""";
    std::string result = run(program);
    assertEqual("function1 with no parameters\nfunction1 with Int parameter\nfunction1 with two Int parameters\n", result, program);
}

void classTest_functionOverloading() {
    std::string program = R""""(
class A {
    method1(): None {
        print("method1 with no parameters")
    }

    method1(a: Int): None {
        print("method1 with Int parameter")
    }

    method1(a: Int, b: Int): None {
        print("method1 with two Int parameters")
    }
}

var a = new A()
a.method1()
a.method1(5)
a.method1(5, 5)
    )"""";
    std::string result = run(program);
    assertEqual("method1 with no parameters\nmethod1 with Int parameter\nmethod1 with two Int parameters\n", result, program);
}


// class Parent {
//     x: Int
//     y: Int
// }
// class MyClass extends Parent {
//     a: Bool
//     b: Int
//     c: Double
//     d: Array<Int>
//     e: String
// }
// var myClass = new MyClass()
// myClass.a = false
// myClass.b = 5
// myClass.c = 6.7
// myClass.d = [1,2,3]
// myClass.e = "abc"
// myClass.x = 77
// myClass.y = 75151515
// print(myClass.a)
// print(myClass.b)
// print(myClass.c)
// print(myClass.d)
// print(myClass.e)
// print(myClass.x)
// print(myClass.y)

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
    classTest_functionOverloading();
    functionOverloading();
    classTest_classProperty();
    classTest_classProperty_Interface();

    functionTakesInterface();
    functionReturnsInterface();

    objectShorthandInitializer();
    inheritedMembers();
    functionTakesBaseClass();
    functionTakesAny();

}
