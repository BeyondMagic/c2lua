#ifndef AST_H
#define AST_H

#include <stddef.h>

typedef enum
{
	TYPE_UNKNOWN = 0,
	TYPE_INT,
	TYPE_CHAR,
	TYPE_FLOAT,
	TYPE_BOOL,
	TYPE_STRING,
	TYPE_ARRAY,
	TYPE_VOID
} TypeKind;

typedef enum
{
	BIN_OP_ADD,
	BIN_OP_SUB,
	BIN_OP_MUL,
	BIN_OP_DIV,
	BIN_OP_MOD,
	BIN_OP_EQ,
	BIN_OP_NEQ,
	BIN_OP_LT,
	BIN_OP_LE,
	BIN_OP_GT,
	BIN_OP_GE,
	BIN_OP_AND,
	BIN_OP_OR
} AstBinaryOp;

typedef enum
{
	UN_OP_POS,
	UN_OP_NEG,
	UN_OP_NOT
} AstUnaryOp;

struct AstExpr;
struct AstStmt;
struct AstBlock;
struct AstFunction;
struct AstProgram;

typedef struct
{
	struct AstExpr **items;
	size_t count;
	size_t capacity;
} AstExprList;

typedef struct
{
	struct AstStmt **items;
	size_t count;
	size_t capacity;
} AstStmtList;

typedef struct
{
	char *name;
	TypeKind type;
} AstParam;

typedef struct
{
	AstParam *items;
	size_t count;
	size_t capacity;
} AstParamList;

typedef struct
{
	struct AstFunction **items;
	size_t count;
	size_t capacity;
} AstFunctionList;

typedef struct AstExpr
{
	enum
	{
		EXPR_INT_LITERAL,
		EXPR_FLOAT_LITERAL,
		EXPR_BOOL_LITERAL,
		EXPR_STRING_LITERAL,
		EXPR_IDENTIFIER,
		EXPR_BINARY,
		EXPR_UNARY,
		EXPR_CALL,
		EXPR_ARRAY_LITERAL,
		EXPR_SUBSCRIPT
	} kind;
	TypeKind type;
	union
	{
		long long int_value;
		double float_value;
		int bool_value;
		char *string_literal;
		char *identifier;
		struct
		{
			AstBinaryOp op;
			struct AstExpr *left;
			struct AstExpr *right;
		} binary;
		struct
		{
			AstUnaryOp op;
			struct AstExpr *operand;
		} unary;
		struct
		{
			char *callee;
			AstExprList args;
		} call;
		struct
		{
			AstExprList elements;
		} array_literal;
		struct
		{
			struct AstExpr *array;
			struct AstExpr *index;
		} subscript;
	} data;
} AstExpr;

typedef struct AstBlock
{
	AstStmtList statements;
} AstBlock;

typedef struct AstStmt
{
	enum
	{
		STMT_BLOCK,
		STMT_DECL,
		STMT_ASSIGN,
		STMT_ARRAY_ASSIGN,
		STMT_WHILE,
		STMT_FOR,
		STMT_EXPR,
		STMT_RETURN
	} kind;
	union
	{
		AstBlock block;
		struct
		{
			TypeKind type;
			char *name;
			AstExpr *init;
			int is_array;
			size_t array_size;
			AstExpr *array_init;
		} decl;
		struct
		{
			char *name;
			AstExpr *value;
			TypeKind type;
		} assign;
		struct
		{
			char *name;
			AstExpr *index;
			AstExpr *value;
			TypeKind element_type;
			size_t array_size;
		} array_assign;
		struct
		{
			AstExpr *condition;
			struct AstStmt *body;
		} while_stmt;
		struct
		{
			struct AstStmt *init;
			AstExpr *condition;
			struct AstStmt *post;
			struct AstStmt *body;
		} for_stmt;
		AstExpr *expr;
	} data;
} AstStmt;

typedef struct AstFunction
{
	TypeKind return_type;
	char *name;
	AstParamList params;
	AstBlock body;
	int has_mandatory_return;
} AstFunction;

typedef struct AstProgram
{
	AstFunctionList functions;
} AstProgram;

AstProgram *ast_program_create(void);
void ast_program_add_function(AstProgram *program, AstFunction *fn);
void ast_program_destroy(AstProgram *program);

AstFunction *ast_function_create(TypeKind return_type, char *name, AstParamList *params, AstBlock *body);

AstParamList ast_param_list_make(void);
void ast_param_list_push(AstParamList *list, AstParam param);
void ast_param_list_destroy(AstParamList *list);

AstExprList ast_expr_list_make(void);
void ast_expr_list_push(AstExprList *list, AstExpr *expr);
void ast_expr_list_destroy(AstExprList *list);

AstStmtList ast_stmt_list_make(void);
void ast_stmt_list_push(AstStmtList *list, AstStmt *stmt);
void ast_stmt_list_destroy(AstStmtList *list);

AstBlock ast_block_from_list(AstStmtList *list);

AstStmt *ast_stmt_make_block(AstBlock *block);
AstStmt *ast_stmt_make_decl(TypeKind type, char *name, AstExpr *init);
AstStmt *ast_stmt_make_assign(char *name, AstExpr *value);
AstStmt *ast_stmt_make_array_decl(TypeKind type, char *name, size_t size, AstExpr *init);
AstStmt *ast_stmt_make_array_assign(char *name, AstExpr *index, AstExpr *value);
AstStmt *ast_stmt_make_while(AstExpr *condition, AstStmt *body);
AstStmt *ast_stmt_make_for(AstStmt *init, AstExpr *condition, AstStmt *post, AstStmt *body);
AstStmt *ast_stmt_make_expr(AstExpr *expr);
AstStmt *ast_stmt_make_return(AstExpr *expr);
AstExpr *ast_expr_make_array_literal(AstExprList *elements);
AstExpr *ast_expr_make_subscript(AstExpr *array, AstExpr *index);

AstExpr *ast_expr_make_int(long long value);
AstExpr *ast_expr_make_char(long long value);
AstExpr *ast_expr_make_float(double value);
AstExpr *ast_expr_make_bool(int value);
AstExpr *ast_expr_make_string(char *value);
AstExpr *ast_expr_make_identifier(char *name);
AstExpr *ast_expr_make_binary(AstBinaryOp op, AstExpr *left, AstExpr *right);
AstExpr *ast_expr_make_unary(AstUnaryOp op, AstExpr *operand);
AstExpr *ast_expr_make_call(char *callee, AstExprList *args);

TypeKind ast_type_from_keyword(const char *kw);
const char *ast_type_name(TypeKind type);

#endif
