#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *xcalloc(size_t n, size_t size)
{
	void *ptr = calloc(n, size);
	if (!ptr)
	{
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	return ptr;
}

static void ensure_capacity(void **buffer, size_t elem_size, size_t *capacity, size_t needed)
{
	if (*capacity >= needed)
	{
		return;
	}
	size_t new_capacity = (*capacity == 0) ? 4 : (*capacity * 2);
	while (new_capacity < needed)
	{
		new_capacity *= 2;
	}
	void *new_buffer = realloc(*buffer, new_capacity * elem_size);
	if (!new_buffer)
	{
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	*buffer = new_buffer;
	*capacity = new_capacity;
}

static void ast_expr_destroy(AstExpr *expr);
static void ast_stmt_destroy(AstStmt *stmt);

AstProgram *ast_program_create(void)
{
	AstProgram *program = xcalloc(1, sizeof(AstProgram));
	return program;
}

void ast_program_add_function(AstProgram *program, AstFunction *fn)
{
	if (!program || !fn)
	{
		return;
	}
	ensure_capacity((void **)&program->functions.items, sizeof(AstFunction *), &program->functions.capacity, program->functions.count + 1);
	program->functions.items[program->functions.count++] = fn;
}

static void ast_function_destroy(AstFunction *fn)
{
	if (!fn)
	{
		return;
	}
	for (size_t i = 0; i < fn->params.count; ++i)
	{
		free(fn->params.items[i].name);
	}
	free(fn->params.items);
	ast_stmt_list_destroy(&fn->body.statements);
	free(fn->name);
	free(fn);
}

void ast_program_destroy(AstProgram *program)
{
	if (!program)
	{
		return;
	}
	for (size_t i = 0; i < program->functions.count; ++i)
	{
		ast_function_destroy(program->functions.items[i]);
	}
	free(program->functions.items);
	free(program);
}

AstFunction *ast_function_create(TypeKind return_type, char *name, AstParamList *params, AstBlock *body)
{
	AstFunction *fn = xcalloc(1, sizeof(AstFunction));
	fn->return_type = return_type;
	fn->name = name;
	if (params)
	{
		fn->params = *params;
	}
	if (body)
	{
		fn->body = *body;
	}
	return fn;
}

AstParamList ast_param_list_make(void)
{
	AstParamList list = {0};
	return list;
}

void ast_param_list_push(AstParamList *list, AstParam param)
{
	ensure_capacity((void **)&list->items, sizeof(AstParam), &list->capacity, list->count + 1);
	list->items[list->count++] = param;
}

void ast_param_list_destroy(AstParamList *list)
{
	if (!list)
	{
		return;
	}
	for (size_t i = 0; i < list->count; ++i)
	{
		free(list->items[i].name);
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

AstExprList ast_expr_list_make(void)
{
	AstExprList list = {0};
	return list;
}

void ast_expr_list_push(AstExprList *list, AstExpr *expr)
{
	ensure_capacity((void **)&list->items, sizeof(AstExpr *), &list->capacity, list->count + 1);
	list->items[list->count++] = expr;
}

static void ast_expr_destroy(AstExpr *expr)
{
	if (!expr)
	{
		return;
	}
	switch (expr->kind)
	{
	case EXPR_IDENTIFIER:
		free(expr->data.identifier);
		break;
	case EXPR_STRING_LITERAL:
		free(expr->data.string_literal);
		break;
	case EXPR_BINARY:
		ast_expr_destroy(expr->data.binary.left);
		ast_expr_destroy(expr->data.binary.right);
		break;
	case EXPR_UNARY:
		ast_expr_destroy(expr->data.unary.operand);
		break;
	case EXPR_CALL:
		free(expr->data.call.callee);
		for (size_t i = 0; i < expr->data.call.args.count; ++i)
		{
			ast_expr_destroy(expr->data.call.args.items[i]);
		}
		free(expr->data.call.args.items);
		break;
	case EXPR_ARRAY_LITERAL:
		ast_expr_list_destroy(&expr->data.array_literal.elements);
		break;
	case EXPR_SUBSCRIPT:
		ast_expr_destroy(expr->data.subscript.array);
		ast_expr_destroy(expr->data.subscript.index);
		break;
	case EXPR_INT_LITERAL:
	case EXPR_FLOAT_LITERAL:
	case EXPR_BOOL_LITERAL:
		break;
	}
	free(expr);
}

void ast_expr_list_destroy(AstExprList *list)
{
	if (!list)
	{
		return;
	}
	for (size_t i = 0; i < list->count; ++i)
	{
		ast_expr_destroy(list->items[i]);
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

AstStmtList ast_stmt_list_make(void)
{
	AstStmtList list = {0};
	return list;
}

void ast_stmt_list_push(AstStmtList *list, AstStmt *stmt)
{
	ensure_capacity((void **)&list->items, sizeof(AstStmt *), &list->capacity, list->count + 1);
	list->items[list->count++] = stmt;
}

void ast_stmt_list_destroy(AstStmtList *list)
{
	if (!list)
	{
		return;
	}
	for (size_t i = 0; i < list->count; ++i)
	{
		ast_stmt_destroy(list->items[i]);
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void ast_stmt_destroy(AstStmt *stmt)
{
	if (!stmt)
	{
		return;
	}
	switch (stmt->kind)
	{
	case STMT_BLOCK:
		ast_stmt_list_destroy(&stmt->data.block.statements);
		break;
	case STMT_DECL:
		free(stmt->data.decl.name);
		if (stmt->data.decl.init)
		{
			ast_expr_destroy(stmt->data.decl.init);
		}
		if (stmt->data.decl.array_init)
		{
			ast_expr_destroy(stmt->data.decl.array_init);
		}
		break;
	case STMT_ASSIGN:
		free(stmt->data.assign.name);
		ast_expr_destroy(stmt->data.assign.value);
		break;
	case STMT_ARRAY_ASSIGN:
		free(stmt->data.array_assign.name);
		ast_expr_destroy(stmt->data.array_assign.index);
		ast_expr_destroy(stmt->data.array_assign.value);
		break;
	case STMT_WHILE:
		ast_expr_destroy(stmt->data.while_stmt.condition);
		ast_stmt_destroy(stmt->data.while_stmt.body);
		break;
	case STMT_FOR:
		ast_stmt_destroy(stmt->data.for_stmt.init);
		ast_expr_destroy(stmt->data.for_stmt.condition);
		ast_stmt_destroy(stmt->data.for_stmt.post);
		ast_stmt_destroy(stmt->data.for_stmt.body);
		break;
	case STMT_EXPR:
	case STMT_RETURN:
		if (stmt->data.expr)
		{
			ast_expr_destroy(stmt->data.expr);
		}
		break;
	}
	free(stmt);
}

AstBlock ast_block_from_list(AstStmtList *list)
{
	AstBlock block = {0};
	if (list)
	{
		block.statements = *list;
		list->items = NULL;
		list->count = 0;
		list->capacity = 0;
	}
	return block;
}

AstStmt *ast_stmt_make_block(AstBlock *block)
{
	AstStmt *stmt = xcalloc(1, sizeof(AstStmt));
	stmt->kind = STMT_BLOCK;
	if (block)
	{
		stmt->data.block = *block;
	}
	else
	{
		stmt->data.block.statements = ast_stmt_list_make();
	}
	return stmt;
}

AstStmt *ast_stmt_make_decl(TypeKind type, char *name, AstExpr *init)
{
	AstStmt *stmt = xcalloc(1, sizeof(AstStmt));
	stmt->kind = STMT_DECL;
	stmt->data.decl.type = type;
	stmt->data.decl.name = name;
	stmt->data.decl.init = init;
	stmt->data.decl.is_array = 0;
	stmt->data.decl.array_size = 0;
	stmt->data.decl.array_init = NULL;
	return stmt;
}

AstStmt *ast_stmt_make_assign(char *name, AstExpr *value)
{
	AstStmt *stmt = xcalloc(1, sizeof(AstStmt));
	stmt->kind = STMT_ASSIGN;
	stmt->data.assign.name = name;
	stmt->data.assign.value = value;
	stmt->data.assign.type = TYPE_UNKNOWN;
	return stmt;
}

AstStmt *ast_stmt_make_array_decl(TypeKind type, char *name, size_t size, AstExpr *init)
{
	AstStmt *stmt = xcalloc(1, sizeof(AstStmt));
	stmt->kind = STMT_DECL;
	stmt->data.decl.type = type;
	stmt->data.decl.name = name;
	stmt->data.decl.init = NULL;
	stmt->data.decl.is_array = 1;
	stmt->data.decl.array_size = size;
	stmt->data.decl.array_init = init;
	return stmt;
}

AstStmt *ast_stmt_make_array_assign(char *name, AstExpr *index, AstExpr *value)
{
	AstStmt *stmt = xcalloc(1, sizeof(AstStmt));
	stmt->kind = STMT_ARRAY_ASSIGN;
	stmt->data.array_assign.name = name;
	stmt->data.array_assign.index = index;
	stmt->data.array_assign.value = value;
	stmt->data.array_assign.element_type = TYPE_UNKNOWN;
	stmt->data.array_assign.array_size = 0;
	return stmt;
}

AstStmt *ast_stmt_make_while(AstExpr *condition, AstStmt *body)
{
	AstStmt *stmt = xcalloc(1, sizeof(AstStmt));
	stmt->kind = STMT_WHILE;
	stmt->data.while_stmt.condition = condition;
	stmt->data.while_stmt.body = body;
	return stmt;
}

AstStmt *ast_stmt_make_for(AstStmt *init, AstExpr *condition, AstStmt *post, AstStmt *body)
{
	AstStmt *stmt = xcalloc(1, sizeof(AstStmt));
	stmt->kind = STMT_FOR;
	stmt->data.for_stmt.init = init;
	stmt->data.for_stmt.condition = condition;
	stmt->data.for_stmt.post = post;
	stmt->data.for_stmt.body = body;
	return stmt;
}

AstStmt *ast_stmt_make_expr(AstExpr *expr)
{
	AstStmt *stmt = xcalloc(1, sizeof(AstStmt));
	stmt->kind = STMT_EXPR;
	stmt->data.expr = expr;
	return stmt;
}

AstStmt *ast_stmt_make_return(AstExpr *expr)
{
	AstStmt *stmt = xcalloc(1, sizeof(AstStmt));
	stmt->kind = STMT_RETURN;
	stmt->data.expr = expr;
	return stmt;
}

AstExpr *ast_expr_make_int(long long value)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_INT_LITERAL;
	expr->type = TYPE_INT;
	expr->data.int_value = value;
	return expr;
}

AstExpr *ast_expr_make_char(long long value)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_INT_LITERAL;
	expr->type = TYPE_CHAR;
	expr->data.int_value = (long long)(unsigned char)value;
	return expr;
}

AstExpr *ast_expr_make_float(double value)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_FLOAT_LITERAL;
	expr->type = TYPE_FLOAT;
	expr->data.float_value = value;
	return expr;
}

AstExpr *ast_expr_make_bool(int value)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_BOOL_LITERAL;
	expr->type = TYPE_BOOL;
	expr->data.bool_value = value ? 1 : 0;
	return expr;
}

AstExpr *ast_expr_make_string(char *value)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_STRING_LITERAL;
	expr->type = TYPE_STRING;
	expr->data.string_literal = value;
	return expr;
}

AstExpr *ast_expr_make_identifier(char *name)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_IDENTIFIER;
	expr->type = TYPE_UNKNOWN;
	expr->data.identifier = name;
	return expr;
}

AstExpr *ast_expr_make_binary(AstBinaryOp op, AstExpr *left, AstExpr *right)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_BINARY;
	expr->type = TYPE_UNKNOWN;
	expr->data.binary.op = op;
	expr->data.binary.left = left;
	expr->data.binary.right = right;
	return expr;
}

AstExpr *ast_expr_make_unary(AstUnaryOp op, AstExpr *operand)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_UNARY;
	expr->type = TYPE_UNKNOWN;
	expr->data.unary.op = op;
	expr->data.unary.operand = operand;
	return expr;
}

AstExpr *ast_expr_make_call(char *callee, AstExprList *args)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_CALL;
	expr->type = TYPE_UNKNOWN;
	expr->data.call.callee = callee;
	if (args)
	{
		expr->data.call.args = *args;
		args->items = NULL;
		args->count = 0;
		args->capacity = 0;
	}
	return expr;
}

AstExpr *ast_expr_make_array_literal(AstExprList *elements)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_ARRAY_LITERAL;
	expr->type = TYPE_ARRAY;
	if (elements)
	{
		expr->data.array_literal.elements = *elements;
		elements->items = NULL;
		elements->count = 0;
		elements->capacity = 0;
	}
	return expr;
}

AstExpr *ast_expr_make_subscript(AstExpr *array, AstExpr *index)
{
	AstExpr *expr = xcalloc(1, sizeof(AstExpr));
	expr->kind = EXPR_SUBSCRIPT;
	expr->type = TYPE_UNKNOWN;
	expr->data.subscript.array = array;
	expr->data.subscript.index = index;
	return expr;
}

TypeKind ast_type_from_keyword(const char *kw)
{
	if (!kw)
	{
		return TYPE_UNKNOWN;
	}
	if (strcmp(kw, "int") == 0)
	{
		return TYPE_INT;
	}
	if (strcmp(kw, "char") == 0)
	{
		return TYPE_CHAR;
	}
	if (strcmp(kw, "float") == 0)
	{
		return TYPE_FLOAT;
	}
	if (strcmp(kw, "bool") == 0)
	{
		return TYPE_BOOL;
	}
	if (strcmp(kw, "string") == 0)
	{
		return TYPE_STRING;
	}
	if (strcmp(kw, "void") == 0)
	{
		return TYPE_VOID;
	}
	return TYPE_UNKNOWN;
}

const char *ast_type_name(TypeKind type)
{
	switch (type)
	{
	case TYPE_INT:
		return "int";
	case TYPE_CHAR:
		return "char";
	case TYPE_FLOAT:
		return "float";
	case TYPE_BOOL:
		return "bool";
	case TYPE_STRING:
		return "string";
	case TYPE_ARRAY:
		return "array";
	case TYPE_VOID:
		return "void";
	case TYPE_UNKNOWN:
	default:
		return "<unknown>";
	}
}
