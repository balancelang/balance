#include <iostream>
#include "ASTTests.h"
#include "../src/Main.h"

using namespace std;

string replaceCharacter(string input, char character, char replaceCharacter) {
    string output = input;
    replace(output.begin(), output.end(), character, replaceCharacter);
    return output;
}

void assertEqual(string expected, string actual, string program) {
    if (actual == expected) {
        cout << "OK   | Program: " << replaceCharacter(program, '\n', ';') << endl;
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

string programToString(string program) {
    antlr4::ANTLRInputStream input(program);
    antlrcpptest::BalanceLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);

    tokens.fill();

    antlrcpptest::BalanceParser parser(&tokens);
    antlr4::tree::ParseTree *tree = parser.root();

    return tree->toStringTree(&parser, false);
}

void createVariablesTestInt() {
    string program = "var a = 555";
    string actual = programToString(program);
    string expected = "(root (rootBlock (lineStatement (statement (assignment var a = (expression (literal (numericLiteral 555))))) <EOF>)))";
    assertEqual(expected, actual, program);
}

void createVariablesTestString() {
    string program = "var a = \"Hello world!\"";
    string actual = programToString(program);
    string expected = "(root (rootBlock (lineStatement (statement (assignment var a = (expression (literal (stringLiteral Hello world!))))) <EOF>)))";
    assertEqual(expected, actual, program);
}

void createVariablesTestBool() {
    string program = "var a = true";
    string actual = programToString(program);
    string expected = "(root (rootBlock (lineStatement (statement (assignment var a = (expression (literal (booleanLiteral true))))) <EOF>)))";
    assertEqual(expected, actual, program);
}

void createVariablesTestFloat() {
    string program = "var a = 5.3";
    string actual = programToString(program);
    string expected = "(root (rootBlock (lineStatement (statement (assignment var a = (expression (literal (doubleLiteral 5.3))))) <EOF>)))";
    assertEqual(expected, actual, program);
}

void createVariablesTestLambda() {
    string program = "var myFunc = () {}";
    string actual = programToString(program);
    string expected = "(root (rootBlock (lineStatement (statement (assignment var myFunc = (expression (lambda ( parameterList ) { functionBlock })))) <EOF>)))";
    assertEqual(expected, actual, program);
}

void runASTTestSuite() {
    // TODO: Set up a real test suite
    puts("RUNNING AST TESTS");
    createVariablesTestInt();
    createVariablesTestString();
    createVariablesTestBool();
    createVariablesTestFloat();
    createVariablesTestLambda();

    // IR tests:
    // Test function with no explicit return type (None)
    // Test function with explicit None return type
    // Test None function with explicit 'return None'
    // Test None function without explicit 'return None'
}