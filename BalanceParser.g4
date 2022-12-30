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
    : newAssignment
    | existingAssignment
    ;

// TODO: Change IDENTIFIER to variableTypeTuple, to allow hinting type (check if can assign to)
existingAssignment
    : IDENTIFIER '=' expression
    ;

newAssignment
    : VAR variableTypeTuple '=' expression
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
    | mapInitializer                                                    # MapInitializerExpression
    ;

mapInitializer
    : '{' LINE_BREAK* mapItemList LINE_BREAK* '}'
    ;

mapItemList
    : (mapItem (COMMA LINE_BREAK* mapItem)*)?
    ;

mapItem
    : key=expression ':' value=expression
    ;

variable
    : IDENTIFIER
    ;

parameterList
	: (variableTypeTuple (COMMA variableTypeTuple)*)?
	;

balanceType
    : (IDENTIFIER | NONE)                                               # SimpleType
    | base=IDENTIFIER '<' typeList '>'                                  # GenericType
    | '(' typeList ')' '->' balanceType                                 # LambdaType
    ;

typeList
    : balanceType (COMMA balanceType)*
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
    : CLASS className=IDENTIFIER classExtendsImplements OPEN_BRACE classElement* CLOSE_BRACE
    ;

// Is there a simpler way to do this?
classExtendsImplements
    : (IMPLEMENTS interfaces=typeList)? (EXTENDS extendedClass=balanceType)?
    | (EXTENDS extendedClass=balanceType)? (IMPLEMENTS interfaces=typeList)?
    ;

classElement
    : classProperty (COMMA | LINE_BREAK)?
    | functionDefinition (COMMA | LINE_BREAK)?
    | LINE_BREAK
    ;

classProperty
    : variableTypeTuple
    ;

variableTypeTuple
    : name=IDENTIFIER (':' type=balanceType)?
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