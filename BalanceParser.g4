parser grammar BalanceParser;

options {
	tokenVocab = BalanceLexer;
}

root
    : rootBlock*;

rootBlock
    : lineStatement
    | functionDefinition (LINE_BREAK | EOF)
    | classDefinition (LINE_BREAK | EOF)
    | interfaceDefinition (LINE_BREAK | EOF)
    ;

lineStatement
    : statement (LINE_BREAK | EOF)
    | importStatement (LINE_BREAK | EOF)
    | LINE_BREAK
    ;

statement
    : assignment
    | expression
    | memberAssignment
    | functionCall
    | classInitializer
    | ifStatement
    | whileStatement
    | returnStatement
    ;

importStatement
    : FROM (IMPORT_PATH | IDENTIFIER) IMPORT importDefinitionList
    ;

importDefinitionList
    : importDefinition (COMMA importDefinition)*
    ;

importDefinition
    : IDENTIFIER                                                        # UnnamedImportDefinition
    // | IDENTIFIER AS name=IDENTIFIER                                     # NamedImportDefinition
    ;

whileStatement
    : WHILE '(' expression ')' '{' ifBlock '}'
    ;

ifStatement
    : IF OPEN_PARENS expression CLOSE_PARENS OPEN_BRACE ifblock=ifBlock CLOSE_BRACE (ELSE OPEN_BRACE elseblock=ifBlock CLOSE_BRACE)?
    ;

ifBlock
    : lineStatement*
    ;

returnStatement
    : RETURN expression?
    ;

// Should IDENTIFIER be variable?
assignment
    : VAR IDENTIFIER '=' expression                                     # NewAssignment
    | IDENTIFIER '=' expression                                         # ExistingAssignment
    ;

// Can these be implemented under 'assignment'?
memberAssignment
    : member=expression '[' index=expression ']' '=' value=expression
    | member=expression '.' access=variable '=' value=expression
    ;

expression
    : lambda                                                            # LambdaExpression
    | member=expression '[' index=expression ']'                        # MemberIndexExpression
    | member=expression '.' access=expression                           # MemberAccessExpression
    | lhs=expression ( '*' | '/' ) rhs=expression                       # MultiplicativeExpression
    | lhs=expression ( '+' | '-' ) rhs=expression                       # AdditiveExpression
    | lhs=expression ('<' | '>' | '<=' | '>=') rhs=expression           # RelationalExpression
    | literal                                                           # LiteralExpression
    | variable                                                          # VariableExpression
    | functionCall                                                      # FunctionCallExpression
    | classInitializer                                                  # ClassInitializerExpression
    ;

variable
    : IDENTIFIER
    ;

parameterList
	: (parameter (COMMA parameter)*)?
	;

balanceType
    : (IDENTIFIER | NONE)                                               # SimpleType
    | base=IDENTIFIER '<' typeList '>'                                  # GenericType
    | '(' typeList ')' '->' balanceType                                 # LambdaType
    ;

typeList
    : balanceType (COMMA balanceType)*
    ;

parameter
    : type=balanceType identifier=IDENTIFIER /* For now, require type */
    ;

argumentList
    : (argument (COMMA argument)*)?
    ;

// TODO: Replace all this with expression?
argument
    : literal
    | expression
    | functionCall
    | classInitializer
    ;

functionCall
    : IDENTIFIER '(' argumentList ')'
    ;

functionSignature
    : IDENTIFIER OPEN_PARENS parameterList CLOSE_PARENS returnType?
    ;

functionDefinition
    : functionSignature WS* OPEN_BRACE LINE_BREAK* functionBlock WS* CLOSE_BRACE WS*
    ;

interfaceDefinition
    : INTERFACE interfaceName=IDENTIFIER WS* OPEN_BRACE LINE_BREAK* interfaceElement* WS* CLOSE_BRACE WS*
    ;

interfaceElement
    : functionSignature
    | LINE_BREAK
    ;

classInitializer
    : NEW IDENTIFIER '(' argumentList ')'
    ;

classDefinition
    : CLASS className=IDENTIFIER classExtendsImplements? OPEN_BRACE classElement* CLOSE_BRACE
    ;

// Add class extension here as well, e.g. as:
// classExtendsImplements
//     : (IMPLEMENTS typeList)? (EXTENDS typeList)?
//     | (EXTENDS typeList)? (IMPLEMENTS typeList)?
//     ;

classExtendsImplements
    : IMPLEMENTS interfaces=typeList
    ;

classElement
    : classProperty LINE_BREAK
    | functionDefinition LINE_BREAK
    | LINE_BREAK
    ;

classProperty
    : type=balanceType name=IDENTIFIER
    ;

lambda
    : OPEN_PARENS parameterList CLOSE_PARENS returnType? '->' OPEN_BRACE WS* functionBlock WS* CLOSE_BRACE WS*
    // | OPEN_PARENS parameterList CLOSE_PARENS '->' expression     // single-expression
    ;

returnType
    : COLON balanceType
    ;

block
    : OPEN_BRACE rootBlock* CLOSE_BRACE
    ;

functionBlock
    : lineStatement*
    ;


literal
    : numericLiteral
    | stringLiteral
    | booleanLiteral
    | doubleLiteral
    | arrayLiteral
    | noneLiteral
    ;

arrayLiteral
    : '[' listElements ','? ']'
    ;

listElements
    : expression? (',' + expression)*
    ;

numericLiteral
    : DECIMAL_INTEGER
    ;

stringLiteral
    : STRING
    ;

booleanLiteral
    : TRUE
    | FALSE
    ;

doubleLiteral
    : DOUBLE
    ;

noneLiteral
    : NONE
    ;