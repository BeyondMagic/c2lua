%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "lexer.h"

static AstProgram *make_program_with_function(AstFunction *fn);
int yyerror(AstProgram **out_program, const char *msg);

extern int yylex(void);
extern FILE *yyin;
static void parser_error_cleanup(AstProgram **out_program);
%}

%code requires {
    #include <stdio.h>
    #include "ast.h"
}

%code provides {
    struct AstProgram;
    AstProgram *c2lua_parse(FILE *input);
}

%parse-param { AstProgram **out_program }

%union {
  long long intValue;
  double floatValue;
  char *id;
  char *string;
  TypeKind type;
  AstExpr *expr;
  AstStmt *stmt;
  AstBlock block;
  AstStmtList stmt_list;
  AstExprList expr_list;
  AstParam param;
  AstParamList param_list;
  AstFunction *function;
  AstProgram *program;
}

%token <intValue> INT_LITERAL CHAR_LITERAL
%token <floatValue> FLOAT_LITERAL
%token <id> IDENT
%token <string> STRING_LITERAL
%token KW_INT KW_CHAR KW_FLOAT KW_BOOL KW_VOID
%token RETURN
%token WHILE FOR
%token TRUE FALSE
%token PLUS MINUS TIMES DIVIDE MOD
%token EQ NEQ LT LE GT GE
%token AND OR NOT
%token ASSIGN
%token COMMA SEMI
%token LPAREN RPAREN
%token LBRACE RBRACE
%token LBRACKET RBRACKET

/* ---------- Precedência ---------- */
%left OR
%left AND
%left PLUS MINUS
%left TIMES DIVIDE MOD
%right NOT
%right UMINUS

%type <program> program function_sequence
%type <function> function_definition
%type <type> type_specifier
%type <param> parameter
%type <param_list> parameter_list_nonempty parameter_list_opt
%type <block> block
%type <stmt> statement compound_statement declaration_statement assignment_statement return_statement expression_statement while_statement for_statement for_init_statement_opt for_post_statement_opt
%type <stmt_list> optional_statement_list statement_list
%type <expr> expression logical_or_expression logical_and_expression equality_expression relational_expression additive_expression multiplicative_expression unary_expression postfix_expression primary_expression array_initializer expression_opt
%type <expr_list> argument_expression_list argument_expression_list_opt initializer_list initializer_list_opt

%start program

%%

program
    : function_sequence
      {
          *out_program = $1;
      }
    ;
function_sequence
    : function_definition
      {
          $$ = make_program_with_function($1);
      }
    | function_sequence function_definition
      {
          ast_program_add_function($1, $2);
          $$ = $1;
      }
    ;

function_definition
    : type_specifier IDENT LPAREN parameter_list_opt RPAREN block
      {
          $$ = ast_function_create($1, $2, &$4, &$6);
      }
    ;

type_specifier
    : KW_INT   { $$ = TYPE_INT; }
    | KW_CHAR  { $$ = TYPE_CHAR; }
    | KW_FLOAT { $$ = TYPE_FLOAT; }
    | KW_BOOL  { $$ = TYPE_BOOL; }
    | KW_VOID  { $$ = TYPE_VOID; }
    ;

parameter_list_opt
    : parameter_list_nonempty { $$ = $1; }
    | /* empty */             { $$ = ast_param_list_make(); }
    ;

parameter_list_nonempty
    : parameter
      {
          AstParamList list = ast_param_list_make();
          ast_param_list_push(&list, $1);
          $$ = list;
      }
    | parameter_list_nonempty COMMA parameter
      {
          ast_param_list_push(&$1, $3);
          $$ = $1;
      }
    ;

parameter
    : type_specifier IDENT
      {
          AstParam param;
          param.type = $1;
          param.name = $2;
          $$ = param;
      }
    ;

block
    : LBRACE optional_statement_list RBRACE
      {
          $$ = ast_block_from_list(&$2);
      }
    ;

optional_statement_list
    : /* empty */
      {
          $$ = ast_stmt_list_make();
      }
    | statement_list
      {
          $$ = $1;
      }
    ;

statement_list
    : statement
      {
          AstStmtList list = ast_stmt_list_make();
          if ($1)
          {
              ast_stmt_list_push(&list, $1);
          }
          $$ = list;
      }
    | statement_list statement
      {
          if ($2)
          {
              ast_stmt_list_push(&$1, $2);
          }
          $$ = $1;
      }
    ;

statement
    : compound_statement
    | declaration_statement
    | assignment_statement
    | return_statement
    | expression_statement
  | while_statement
  | for_statement
    ;

compound_statement
    : block
      {
          $$ = ast_stmt_make_block(&$1);
      }
    ;

declaration_statement
    : type_specifier IDENT ASSIGN expression SEMI
      {
          $$ = ast_stmt_make_decl($1, $2, $4);
      }
    | type_specifier IDENT SEMI
      {
          $$ = ast_stmt_make_decl($1, $2, NULL);
      }
    | type_specifier IDENT LBRACKET INT_LITERAL RBRACKET SEMI
      {
          $$ = ast_stmt_make_array_decl($1, $2, (size_t)$4, NULL);
      }
    | type_specifier IDENT LBRACKET INT_LITERAL RBRACKET ASSIGN array_initializer SEMI
      {
          $$ = ast_stmt_make_array_decl($1, $2, (size_t)$4, $7);
      }
    ;

assignment_statement
    : IDENT ASSIGN expression SEMI
      {
          $$ = ast_stmt_make_assign($1, $3);
      }
    | IDENT LBRACKET expression RBRACKET ASSIGN expression SEMI
      {
          $$ = ast_stmt_make_array_assign($1, $3, $6);
      }
    ;

return_statement
    : RETURN expression SEMI
      {
          $$ = ast_stmt_make_return($2);
      }
    | RETURN SEMI
      {
          $$ = ast_stmt_make_return(NULL);
      }
    ;

expression_statement
    : expression SEMI
      {
          $$ = ast_stmt_make_expr($1);
      }
    ;

while_statement
    : WHILE LPAREN expression RPAREN statement
      {
          $$ = ast_stmt_make_while($3, $5);
      }
    ;

for_statement
    : FOR LPAREN for_init_statement_opt SEMI expression_opt SEMI for_post_statement_opt RPAREN statement
      {
          $$ = ast_stmt_make_for($3, $5, $7, $9);
      }
    ;

for_init_statement_opt
    : type_specifier IDENT ASSIGN expression
      {
          $$ = ast_stmt_make_decl($1, $2, $4);
      }
    | type_specifier IDENT
      {
          $$ = ast_stmt_make_decl($1, $2, NULL);
      }
    | IDENT ASSIGN expression
      {
          $$ = ast_stmt_make_assign($1, $3);
      }
    | IDENT LBRACKET expression RBRACKET ASSIGN expression
      {
          $$ = ast_stmt_make_array_assign($1, $3, $6);
      }
    | /* empty */
      {
          $$ = NULL;
      }
    ;

for_post_statement_opt
    : IDENT ASSIGN expression
      {
          $$ = ast_stmt_make_assign($1, $3);
      }
    | IDENT LBRACKET expression RBRACKET ASSIGN expression
      {
          $$ = ast_stmt_make_array_assign($1, $3, $6);
      }
    | /* empty */
      {
          $$ = NULL;
      }
    ;

expression_opt
    : expression
      {
          $$ = $1;
      }
    | /* empty */
      {
          $$ = NULL;
      }
    ;

expression
    : logical_or_expression
      {
          $$ = $1;
      }
    ;

logical_or_expression
    : logical_and_expression
      {
          $$ = $1;
      }
    | logical_or_expression OR logical_and_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_OR, $1, $3);
      }
    ;

logical_and_expression
    : equality_expression
      {
          $$ = $1;
      }
    | logical_and_expression AND equality_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_AND, $1, $3);
      }
    ;

equality_expression
    : relational_expression
      {
          $$ = $1;
      }
    | equality_expression EQ relational_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_EQ, $1, $3);
      }
    | equality_expression NEQ relational_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_NEQ, $1, $3);
      }
    ;

relational_expression
    : additive_expression
      {
          $$ = $1;
      }
    | relational_expression LT additive_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_LT, $1, $3);
      }
    | relational_expression LE additive_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_LE, $1, $3);
      }
    | relational_expression GT additive_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_GT, $1, $3);
      }
    | relational_expression GE additive_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_GE, $1, $3);
      }
    ;

additive_expression
    : multiplicative_expression
      {
          $$ = $1;
      }
    | additive_expression PLUS multiplicative_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_ADD, $1, $3);
      }
    | additive_expression MINUS multiplicative_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_SUB, $1, $3);
      }
    ;

multiplicative_expression
    : unary_expression
      {
          $$ = $1;
      }
    | multiplicative_expression TIMES unary_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_MUL, $1, $3);
      }
    | multiplicative_expression DIVIDE unary_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_DIV, $1, $3);
      }
    | multiplicative_expression MOD unary_expression
      {
          $$ = ast_expr_make_binary(BIN_OP_MOD, $1, $3);
      }
    ;

unary_expression
    : postfix_expression
      {
          $$ = $1;
      }
    | MINUS unary_expression %prec UMINUS
      {
          $$ = ast_expr_make_unary(UN_OP_NEG, $2);
      }
    | PLUS unary_expression %prec UMINUS
      {
          $$ = ast_expr_make_unary(UN_OP_POS, $2);
      }
    | NOT unary_expression
      {
          $$ = ast_expr_make_unary(UN_OP_NOT, $2);
      }
    ;

postfix_expression
    : primary_expression
      {
          $$ = $1;
      }
    | IDENT LPAREN argument_expression_list_opt RPAREN
      {
          $$ = ast_expr_make_call($1, &$3);
      }
    | postfix_expression LBRACKET expression RBRACKET
      {
          $$ = ast_expr_make_subscript($1, $3);
      }
    ;

argument_expression_list_opt
    : argument_expression_list
      {
          $$ = $1;
      }
    | /* empty */
      {
          $$ = ast_expr_list_make();
      }
    ;

argument_expression_list
    : expression
      {
          AstExprList list = ast_expr_list_make();
          ast_expr_list_push(&list, $1);
          $$ = list;
      }
    | argument_expression_list COMMA expression
      {
          ast_expr_list_push(&$1, $3);
          $$ = $1;
      }
    ;

primary_expression
    : INT_LITERAL
      {
          $$ = ast_expr_make_int($1);
      }
    | CHAR_LITERAL
      {
          $$ = ast_expr_make_char($1);
      }
    | FLOAT_LITERAL
      {
          $$ = ast_expr_make_float($1);
      }
    | TRUE
      {
          $$ = ast_expr_make_bool(1);
      }
    | FALSE
      {
          $$ = ast_expr_make_bool(0);
      }
    | IDENT
      {
          $$ = ast_expr_make_identifier($1);
      }
    | STRING_LITERAL
      {
          $$ =  ast_expr_make_string($1);
      }
    | LPAREN expression RPAREN
      {
          $$ = $2;
      }
    ;

array_initializer
    : LBRACE initializer_list_opt RBRACE
      {
          $$ = ast_expr_make_array_literal(&$2);
      }
    ;

initializer_list_opt
    : initializer_list
      {
          $$ = $1;
      }
    | /* empty */
      {
          $$ = ast_expr_list_make();
      }
    ;

initializer_list
    : expression
      {
          AstExprList list = ast_expr_list_make();
          ast_expr_list_push(&list, $1);
          $$ = list;
      }
    | initializer_list COMMA expression
      {
          ast_expr_list_push(&$1, $3);
          $$ = $1;
      }
    ;

%%

static AstProgram *make_program_with_function(AstFunction *fn)
{
    AstProgram *program = ast_program_create();
    ast_program_add_function(program, fn);
    return program;
}

static void parser_error_cleanup(AstProgram **out_program)
{
    if (out_program && *out_program)
    {
        ast_program_destroy(*out_program);
        *out_program = NULL;
    }
}

int yyerror(AstProgram **out_program, const char *msg)
{
    (void)out_program;
    fprintf(stderr,
        "%s:%d:%d: syntax error: %s\n",
        lexer_get_source_name(),
        yy_token_line,
        yy_token_column,
        msg ? msg : "unknown");
    return 0;
}

AstProgram *c2lua_parse(FILE *input)
{
    AstProgram *program = NULL;
    yyin = input;
    if (yyparse(&program) != 0)
    {
        parser_error_cleanup(&program);
        return NULL;
    }
    return program;
}
