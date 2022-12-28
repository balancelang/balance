#include <iostream>
#include "ASTTests.h"
#include "antlr4-runtime.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

// using namespace antlr4;
// using namespace antlrcpptest;

std::string programToString(std::string program) {
    antlr4::ANTLRInputStream input(program);
    antlrcpptest::BalanceLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);

    tokens.fill();

    antlrcpptest::BalanceParser parser(&tokens);
    antlr4::tree::ParseTree *tree = parser.root();

    return tree->toStringTree(&parser, false);
}

void createVariablesTestInt() {
    std::string program = "var a = 555";
    std::string actual = programToString(program);
    std::string expected = "(root (rootBlock (lineStatement (statement (assignment (newAssignment var (variableTypeTuple a) = (expression (literal (numericLiteral 555)))))) <EOF>)))";
    assertEqual(expected, actual, program);
}

void createVariablesTestString() {
    std::string program = "var a = \"Hello world!\"";
    std::string actual = programToString(program);
    std::string expected = "(root (rootBlock (lineStatement (statement (assignment (newAssignment var (variableTypeTuple a) = (expression (literal (stringLiteral Hello world!)))))) <EOF>)))";
    assertEqual(expected, actual, program);
}

void createVariablesTestBool() {
    std::string program = "var a = true";
    std::string actual = programToString(program);
    std::string expected = "(root (rootBlock (lineStatement (statement (assignment (newAssignment var (variableTypeTuple a) = (expression (literal (booleanLiteral true)))))) <EOF>)))";
    assertEqual(expected, actual, program);
}

void createVariablesTestFloat() {
    std::string program = "var a = 5.3";
    std::string actual = programToString(program);
    std::string expected = "(root (rootBlock (lineStatement (statement (assignment (newAssignment var (variableTypeTuple a) = (expression (literal (doubleLiteral 5.3)))))) <EOF>)))";
    assertEqual(expected, actual, program);
}

void createVariablesTestLambda() {
    std::string program = "var myFunc = () -> {}";
    std::string actual = programToString(program);
    std::string expected = "(root (rootBlock (lineStatement (statement (assignment (newAssignment var (variableTypeTuple myFunc) = (expression (lambda ( parameterList ) -> { functionBlock }))))) <EOF>)))";
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