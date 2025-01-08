# wlp4-compiler

The compiler works in the following steps:

1. Scanning: The **scanner** scans and tokenizes the code
2. Parsing: constructing a parse tree from the scanned tokens. Check for syntax and semantics.
3. Type-checking: annotate the parse tree with types and check for type correctness.
4. Codegen: generate MIPS assembly code from the annotated parse tree.

## Directories with respect to steps

scanner -> parse -> type -> codegen

## Sample

WLP4 Code:

```C
int wain(int nn, int b) {
  if(nn < b) {
    while(nn < b) {
      nn = nn + 1;
    }
  } else {
    nn = 241;
  }
  return nn;
}
```

Tokenization:

```
INT int
WAIN wain
LPAREN (
INT int
ID nn
COMMA ,
INT int
ID b
RPAREN )
LBRACE {
IF if
LPAREN (
ID nn
LT <
ID b
RPAREN )
LBRACE {
WHILE while
LPAREN (
ID nn
LT <
ID b
RPAREN )
LBRACE {
ID nn
BECOMES =
ID nn
PLUS +
NUM 1
SEMI ;
RBRACE }
RBRACE }
ELSE else
LBRACE {
ID nn
BECOMES =
NUM 241
SEMI ;
RBRACE }
RETURN return
ID nn
SEMI ;
RBRACE }

```

Annotated parse tree

```
start BOF procedures EOF
BOF BOF
procedures main
main INT WAIN LPAREN dcl COMMA dcl RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE
INT int
WAIN wain
LPAREN (
dcl type ID
type INT
INT int
ID nn : int
COMMA ,
dcl type ID
type INT
INT int
ID b : int
RPAREN )
LBRACE {
dcls .EMPTY
statements statements statement
statements .EMPTY
statement IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE
IF if
LPAREN (
test expr LT expr
expr term : int
term factor : int
factor ID : int
ID nn : int
LT <
expr term : int
term factor : int
factor ID : int
ID b : int
RPAREN )
LBRACE {
statements statements statement
statements .EMPTY
statement WHILE LPAREN test RPAREN LBRACE statements RBRACE
WHILE while
LPAREN (
test expr LT expr
expr term : int
term factor : int
factor ID : int
ID nn : int
LT <
expr term : int
term factor : int
factor ID : int
ID b : int
RPAREN )
LBRACE {
statements statements statement
statements .EMPTY
statement lvalue BECOMES expr SEMI
lvalue ID : int
ID nn : int
BECOMES =
expr expr PLUS term : int
expr term : int
term factor : int
factor ID : int
ID nn : int
PLUS +
term factor : int
factor NUM : int
NUM 1 : int
SEMI ;
RBRACE }
RBRACE }
ELSE else
LBRACE {
statements statements statement
statements .EMPTY
statement lvalue BECOMES expr SEMI
lvalue ID : int
ID nn : int
BECOMES =
expr term : int
term factor : int
factor NUM : int
NUM 241 : int
SEMI ;
RBRACE }
RETURN return
expr term : int
term factor : int
factor ID : int
ID nn : int
SEMI ;
RBRACE }
EOF EOF

```

MIPS Output:

```
lis $4
.word 4
lis $11
.word 1
sw $1, -4($30)
sub $30, $30, $4
sw $2, -4($30)
sub $30, $30, $4
sub $29, $30, $4
lw $3, 8($29)
sw $3, -4($30)
sub $30, $30, $4
lw $3, 4($29)
add $30, $30, $4
lw $5, -4($30)
slt $3, $5, $3
beq $3, $0, ELSE0
WHILE0:
lw $3, 8($29)
sw $3, -4($30)
sub $30, $30, $4
lw $3, 4($29)
add $30, $30, $4
lw $5, -4($30)
slt $3, $5, $3
beq $3, $0, ENDWHILE0
lw $3, 8($29)
sw $3, -4($30)
sub $30, $30, $4
lis $3
.word 1
add $30, $30, $4
lw $5, -4($30)
add $3, $5, $3
sw $3, 8($29)
beq $0, $0, WHILE0
ENDWHILE0:
beq $0, $0, ENDIF0
ELSE0:
lis $3
.word 241
sw $3, 8($29)
ENDIF0:
lw $3, 8($29)
add $30, $30, $4
add $30, $30, $4
jr $31
```
