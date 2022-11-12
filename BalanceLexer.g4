lexer grammar BalanceLexer;

SingleLineComment   : '//' ~[\r\n]* -> channel(HIDDEN);

DEF                 : 'def';
RETURN              : 'return';
IF                  : 'if';
ELSE                : 'else';
VAR                 : 'var';
AND                 : 'and';
OR                  : 'or';
FALSE               : 'false';
TRUE                : 'true';
WHILE               : 'while';
CLASS               : 'class';
NEW                 : 'new';
FROM                : 'from';
IMPORT              : 'import';
AS                  : 'as';
NONE                : 'None';


OPEN_BRACE          : '{';
CLOSE_BRACE         : '}';
OPEN_BRACKET        : '[';
CLOSE_BRACKET       : ']';
OPEN_PARENS         : '(';
CLOSE_PARENS        : ')';
DOT                 : '.';

COMMA               : ',';
SEMICOLON           : ';';
PLUS                : '+';
MINUS               : '-';
STAR                : '*';
DIV                 : '/';
BANG                : '!';
ASSIGNMENT          : '=';
LT                  : '<';
GT                  : '>';
OP_PTR              : '->';
OP_EQ               : '==';
OP_NE               : '!=';
OP_LE               : '<=';
OP_GE               : '>=';

DOUBLE
    : '-'? [0-9]+ '.' [0-9]+
    ;

DECIMAL_INTEGER
    : '0'
    | '-'? [1-9] [0-9]*
    ;

LINE_BREAK
    : '\n'
    | '\r\n'
    ;

WS: [ \t]+ -> channel(HIDDEN);

IDENTIFIER: [a-zA-Z_]+ [a-zA-Z_0-9]*;

IMPORT_PATH: '/'? ([a-zA-Z_]+ [a-zA-Z_0-9]* '/'?)+;

STRING: '"' ~["]* '"' {setText(getText().substr(1, getText().length()-2));};
// STRING: '"' [a-zA-Z_ 0-9!#¤%&/()=?\\`',.]* '"' {setText(getText().substr(1, getText().length()-2));};


// STRING
//     : (SINGLE_LINE_STRING | MULTI_LINE_STRING)
//     ;



fragment SINGLE_LINE_STRING
    : '"' (. | ~[\r\n"])*? '"'
    | '\'' (. | ~[\r\n'])*? '\''
    ;

fragment MULTI_LINE_STRING
    : '"""' (.)*? '"""'
    | '\'\'\'' (.)*? '\'\'\''
    ;