#include "semantic.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void semantic_error(const char *fmt, ...);
static int analyze_function(SemanticInfo *info, AstFunction *fn);
static int analyze_block(SemanticInfo *info, AstFunction *fn, SymbolTable *symbols, AstBlock *block, int push_scope, int *has_return);
static int analyze_statement(SemanticInfo *info, AstFunction *fn, SymbolTable *symbols, AstStmt *stmt, int *has_return);
static TypeKind analyze_expression(SemanticInfo *info, SymbolTable *symbols, AstExpr *expr);
static int ensure_assignable(TypeKind target, TypeKind value);
static int is_numeric(TypeKind type);
static int is_boolean_like(TypeKind type);
static TypeKind arithmetic_result(TypeKind left, TypeKind right);
static TypeKind analyze_builtin_call(SemanticInfo *info, SymbolTable *symbols, AstExpr *expr);

static int semantic_error_flag = 0;

int semantic_analyze(AstProgram *program, SemanticInfo *info)
{
	if (!program || !info)
	{
		return 0;
	}

	semantic_error_flag = 0;
	function_table_init(&info->functions);

	for (size_t i = 0; i < program->functions.count; ++i)
	{
		AstFunction *fn = program->functions.items[i];
		if (!function_table_add(&info->functions, fn->name, fn->return_type, &fn->params))
		{
			semantic_error("duplicated function '%s'", fn->name);
			semantic_info_free(info);
			return 0;
		}
	}

	int ok = 1;
	for (size_t i = 0; i < program->functions.count && ok; ++i)
	{
		ok = analyze_function(info, program->functions.items[i]);
	}

	if (!ok || semantic_error_flag)
	{
		semantic_info_free(info);
		return 0;
	}

	return 1;
}

void semantic_info_free(SemanticInfo *info)
{
	if (!info)
	{
		return;
	}
	function_table_free(&info->functions);
}

static void semantic_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fputs("semantic error: ", stderr);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	va_end(args);
	semantic_error_flag = 1;
}

static int analyze_function(SemanticInfo *info, AstFunction *fn)
{
	SymbolTable symbols;
	symbol_table_init(&symbols);
	symbol_table_push_scope(&symbols);

	for (size_t i = 0; i < fn->params.count; ++i)
	{
		AstParam *param = &fn->params.items[i];
		if (param->type == TYPE_VOID)
		{
			semantic_error("parameter '%s' in function '%s' cannot be void", param->name, fn->name);
			symbol_table_free(&symbols);
			return 0;
		}
		if (!symbol_table_add(&symbols, param->name, param->type, 0, 0, TYPE_UNKNOWN, NULL))
		{
			semantic_error("duplicate parameter '%s' in function '%s'", param->name, fn->name);
			symbol_table_free(&symbols);
			return 0;
		}
	}

	int has_return = (fn->return_type == TYPE_VOID) ? 1 : 0;
	if (!analyze_block(info, fn, &symbols, &fn->body, 0, &has_return))
	{
		symbol_table_free(&symbols);
		return 0;
	}

	if (fn->return_type != TYPE_VOID && !has_return)
	{
		semantic_error("function '%s' must return a value", fn->name);
		symbol_table_free(&symbols);
		return 0;
	}

	fn->has_mandatory_return = has_return ? 1 : 0;
	symbol_table_free(&symbols);
	return 1;
}

static int analyze_block(SemanticInfo *info, AstFunction *fn, SymbolTable *symbols, AstBlock *block, int push_scope, int *has_return)
{
	if (!block)
	{
		return 1;
	}

	if (push_scope)
	{
		symbol_table_push_scope(symbols);
	}

	for (size_t i = 0; i < block->statements.count; ++i)
	{
		AstStmt *stmt = block->statements.items[i];
		if (!analyze_statement(info, fn, symbols, stmt, has_return))
		{
			if (push_scope)
			{
				symbol_table_pop_scope(symbols);
			}
			return 0;
		}
	}

	if (push_scope)
	{
		symbol_table_pop_scope(symbols);
	}

	return 1;
}

static int analyze_statement(SemanticInfo *info, AstFunction *fn, SymbolTable *symbols, AstStmt *stmt, int *has_return)
{
	if (!stmt)
	{
		return 1;
	}

	switch (stmt->kind)
	{
	case STMT_BLOCK:
		if (!analyze_block(info, fn, symbols, &stmt->data.block, 1, has_return))
		{
			return 0;
		}
		break;
	case STMT_DECL:
	{
		if (stmt->data.decl.type == TYPE_VOID)
		{
			semantic_error("variable '%s' in function '%s' cannot be void", stmt->data.decl.name, fn->name);
			return 0;
		}
		if (stmt->data.decl.is_array)
		{
			if (stmt->data.decl.array_size == 0)
			{
				semantic_error("array '%s' in function '%s' must have size greater than zero",
							   stmt->data.decl.name,
							   fn->name);
				return 0;
			}
			if (!symbol_table_add(symbols,
								  stmt->data.decl.name,
								  TYPE_ARRAY,
								  1,
								  stmt->data.decl.array_size,
								  stmt->data.decl.type,
								  stmt))
			{
				semantic_error("duplicate declaration of '%s' in function '%s'", stmt->data.decl.name, fn->name);
				return 0;
			}
			if (stmt->data.decl.array_init)
			{
				AstExpr *init = stmt->data.decl.array_init;
				if (!init || init->kind != EXPR_ARRAY_LITERAL)
				{
					semantic_error("array '%s' initializer must be an array literal", stmt->data.decl.name);
					return 0;
				}
				size_t count = init->data.array_literal.elements.count;
				if (count > stmt->data.decl.array_size)
				{
					semantic_error("array '%s' initializer has too many elements", stmt->data.decl.name);
					return 0;
				}
				for (size_t i = 0; i < count; ++i)
				{
					AstExpr *elem = init->data.array_literal.elements.items[i];
					TypeKind elem_type = analyze_expression(info, symbols, elem);
					elem->type = elem_type;
					if (!ensure_assignable(stmt->data.decl.type, elem_type))
					{
						semantic_error("initializer %zu for array '%s' expected %s but got %s",
									   i + 1,
									   stmt->data.decl.name,
									   ast_type_name(stmt->data.decl.type),
									   ast_type_name(elem_type));
						return 0;
					}
				}
			}
		}
		else
		{
			if (!symbol_table_add(symbols, stmt->data.decl.name, stmt->data.decl.type, 0, 0, TYPE_UNKNOWN, stmt))
			{
				semantic_error("duplicate declaration of '%s' in function '%s'", stmt->data.decl.name, fn->name);
				return 0;
			}
			if (stmt->data.decl.init)
			{
				TypeKind expr_type = analyze_expression(info, symbols, stmt->data.decl.init);
				stmt->data.decl.init->type = expr_type;
				if (!ensure_assignable(stmt->data.decl.type, expr_type))
				{
					semantic_error("cannot initialize '%s' of type %s with expression of type %s in function '%s'",
								   stmt->data.decl.name,
								   ast_type_name(stmt->data.decl.type),
								   ast_type_name(expr_type),
								   fn->name);
					return 0;
				}
			}
		}
		break;
	}
	case STMT_ASSIGN:
	{
		const Symbol *symbol = symbol_table_lookup(symbols, stmt->data.assign.name);
		if (!symbol)
		{
			semantic_error("assignment to undeclared identifier '%s' in function '%s'",
						   stmt->data.assign.name,
						   fn->name);
			return 0;
		}
		stmt->data.assign.type = symbol->type;
		TypeKind expr_type = analyze_expression(info, symbols, stmt->data.assign.value);
		stmt->data.assign.value->type = expr_type;
		if (!ensure_assignable(symbol->type, expr_type))
		{
			semantic_error("cannot assign expression of type %s to variable '%s' of type %s in function '%s'",
						   ast_type_name(expr_type),
						   stmt->data.assign.name,
						   ast_type_name(symbol->type),
						   fn->name);
			return 0;
		}
		break;
	}
	case STMT_ARRAY_ASSIGN:
	{
		const Symbol *symbol = symbol_table_lookup(symbols, stmt->data.array_assign.name);
		if (!symbol)
		{
			semantic_error("assignment to undeclared identifier '%s' in function '%s'",
						   stmt->data.array_assign.name,
						   fn->name);
			return 0;
		}
		if (!symbol->is_array)
		{
			semantic_error("identifier '%s' in function '%s' is not an array",
						   stmt->data.array_assign.name,
						   fn->name);
			return 0;
		}
		TypeKind index_type = analyze_expression(info, symbols, stmt->data.array_assign.index);
		stmt->data.array_assign.index->type = index_type;
		if (index_type != TYPE_INT)
		{
			semantic_error("array index for '%s' must be integer", stmt->data.array_assign.name);
			return 0;
		}
		TypeKind value_type = analyze_expression(info, symbols, stmt->data.array_assign.value);
		stmt->data.array_assign.value->type = value_type;
		if (!ensure_assignable(symbol->element_type, value_type))
		{
			semantic_error("cannot assign expression of type %s to element of '%s' (type %s)",
						   ast_type_name(value_type),
						   stmt->data.array_assign.name,
						   ast_type_name(symbol->element_type));
			return 0;
		}
		stmt->data.array_assign.element_type = symbol->element_type;
		stmt->data.array_assign.array_size = symbol->array_size;
		break;
	}
	case STMT_WHILE:
	{
		TypeKind cond_type = analyze_expression(info, symbols, stmt->data.while_stmt.condition);
		stmt->data.while_stmt.condition->type = cond_type;
		if (!is_boolean_like(cond_type))
		{
			semantic_error("while condition in function '%s' must be boolean-compatible but found %s",
						   fn->name,
						   ast_type_name(cond_type));
			return 0;
		}
		int loop_has_return = 0;
		if (!analyze_statement(info, fn, symbols, stmt->data.while_stmt.body, &loop_has_return))
		{
			return 0;
		}
		break;
	}
	case STMT_FOR:
	{
		symbol_table_push_scope(symbols);
		int ok = 1;
		if (stmt->data.for_stmt.init && ok)
		{
			ok = analyze_statement(info, fn, symbols, stmt->data.for_stmt.init, has_return);
		}
		if (stmt->data.for_stmt.condition && ok)
		{
			TypeKind cond_type = analyze_expression(info, symbols, stmt->data.for_stmt.condition);
			stmt->data.for_stmt.condition->type = cond_type;
			if (!is_boolean_like(cond_type))
			{
				semantic_error("for condition in function '%s' must be boolean-compatible but found %s",
							   fn->name,
							   ast_type_name(cond_type));
				ok = 0;
			}
		}
		if (stmt->data.for_stmt.body && ok)
		{
			int loop_has_return = 0;
			ok = analyze_statement(info, fn, symbols, stmt->data.for_stmt.body, &loop_has_return);
		}
		if (stmt->data.for_stmt.post && ok)
		{
			int post_has_return = 0;
			ok = analyze_statement(info, fn, symbols, stmt->data.for_stmt.post, &post_has_return);
		}
		symbol_table_pop_scope(symbols);
		if (!ok)
		{
			return 0;
		}
		break;
	}

	case STMT_IF:
	{
		TypeKind cond_type = analyze_expression(info, symbols, stmt->data.if_stmt.condition);
		stmt->data.if_stmt.condition->type = cond_type;

		if (!is_boolean_like(cond_type))
		{
			semantic_error("if condition in function '%s' must be boolean-compatible but found %s",
						   fn->name,
						   ast_type_name(cond_type));
			return 0;
		}

		int then_has_return = 0;
		if (!analyze_statement(info, fn, symbols, stmt->data.if_stmt.then_branch, &then_has_return))
			return 0;

		int else_has_return = 0;
		if (stmt->data.if_stmt.else_branch)
		{
			if (!analyze_statement(info, fn, symbols, stmt->data.if_stmt.else_branch, &else_has_return))
				return 0;
		}

		if (then_has_return && stmt->data.if_stmt.else_branch && else_has_return)
			*has_return = 1;

		break;
	}

	case STMT_EXPR:
		if (stmt->data.expr)
		{
			analyze_expression(info, symbols, stmt->data.expr);
		}
		break;
	case STMT_RETURN:
	{
		if (stmt->data.expr)
		{
			TypeKind expr_type = analyze_expression(info, symbols, stmt->data.expr);
			stmt->data.expr->type = expr_type;
			if (fn->return_type == TYPE_VOID)
			{
				semantic_error("void function '%s' should not return a value", fn->name);
				return 0;
			}
			if (!ensure_assignable(fn->return_type, expr_type))
			{
				semantic_error("return type mismatch in function '%s': expected %s but found %s",
							   fn->name,
							   ast_type_name(fn->return_type),
							   ast_type_name(expr_type));
				return 0;
			}
			*has_return = 1;
		}
		else
		{
			if (fn->return_type != TYPE_VOID)
			{
				semantic_error("function '%s' must return a value", fn->name);
				return 0;
			}
			*has_return = 1;
		}
		break;
	}
	}

	return 1;
}

static TypeKind analyze_expression(SemanticInfo *info, SymbolTable *symbols, AstExpr *expr)
{
	if (!expr)
	{
		return TYPE_UNKNOWN;
	}

	switch (expr->kind)
	{
	case EXPR_INT_LITERAL:
		if (expr->type != TYPE_CHAR)
		{
			expr->type = TYPE_INT;
		}
		return expr->type;
	case EXPR_FLOAT_LITERAL:
		expr->type = TYPE_FLOAT;
		return expr->type;
	case EXPR_BOOL_LITERAL:
		expr->type = TYPE_BOOL;
		return expr->type;
	case EXPR_STRING_LITERAL:
		expr->type = TYPE_STRING;
		return expr->type;
	case EXPR_ARRAY_LITERAL:
	{
		for (size_t i = 0; i < expr->data.array_literal.elements.count; ++i)
		{
			AstExpr *elem = expr->data.array_literal.elements.items[i];
			TypeKind elem_type = analyze_expression(info, symbols, elem);
			elem->type = elem_type;
		}
		expr->type = TYPE_ARRAY;
		return expr->type;
	}
	case EXPR_IDENTIFIER:
	{
		const Symbol *symbol = symbol_table_lookup(symbols, expr->data.identifier);
		if (!symbol)
		{
			semantic_error("use of undeclared identifier '%s'", expr->data.identifier);
			expr->type = TYPE_UNKNOWN;
			return expr->type;
		}
		if (symbol->stmt_ref)
		{
			symbol->stmt_ref->data.decl.is_used = 1;
		}

		expr->type = symbol->type;
		return expr->type;
	}
	case EXPR_SUBSCRIPT:
	{
		AstExpr *array_expr = expr->data.subscript.array;
		AstExpr *index_expr = expr->data.subscript.index;
		TypeKind array_type = analyze_expression(info, symbols, array_expr);
		array_expr->type = array_type;
		if (array_expr->kind != EXPR_IDENTIFIER)
		{
			semantic_error("array subscript base must be an identifier");
			expr->type = TYPE_UNKNOWN;
			return expr->type;
		}
		const Symbol *symbol = symbol_table_lookup(symbols, array_expr->data.identifier);
		if (!symbol)
		{
			semantic_error("use of undeclared identifier '%s'", array_expr->data.identifier);
			expr->type = TYPE_UNKNOWN;
			return expr->type;
		}
		if (!symbol->is_array)
		{
			semantic_error("identifier '%s' is not an array", array_expr->data.identifier);
			expr->type = TYPE_UNKNOWN;
			return expr->type;
		}
		TypeKind index_type = analyze_expression(info, symbols, index_expr);
		index_expr->type = index_type;
		if (index_type != TYPE_INT)
		{
			semantic_error("array index for '%s' must be integer", array_expr->data.identifier);
		}
		expr->type = symbol->element_type;
		return expr->type;
	}
	case EXPR_UNARY:
	{
		TypeKind operand_type = analyze_expression(info, symbols, expr->data.unary.operand);
		expr->data.unary.operand->type = operand_type;
		if (expr->data.unary.op == UN_OP_NEG || expr->data.unary.op == UN_OP_POS)
		{
			if (!is_numeric(operand_type))
			{
				semantic_error("unary operator expects numeric operand");
			}
			expr->type = operand_type;
		}
		else if (expr->data.unary.op == UN_OP_NOT)
		{
			if (!is_boolean_like(operand_type))
			{
				semantic_error("logical not expects boolean or numeric operand");
			}
			expr->type = TYPE_BOOL;
		}
		else
		{
			expr->type = operand_type;
		}
		return expr->type;
	}
	case EXPR_BINARY:
	{
		TypeKind left_type = analyze_expression(info, symbols, expr->data.binary.left);
		TypeKind right_type = analyze_expression(info, symbols, expr->data.binary.right);
		expr->data.binary.left->type = left_type;
		expr->data.binary.right->type = right_type;

		switch (expr->data.binary.op)
		{
		case BIN_OP_ADD:
		case BIN_OP_SUB:
		case BIN_OP_MUL:
		case BIN_OP_DIV:
			if (!is_numeric(left_type) || !is_numeric(right_type))
			{
				semantic_error("arithmetic operator expects numeric operands");
			}
			expr->type = arithmetic_result(left_type, right_type);
			return expr->type;
		case BIN_OP_MOD:
			if (left_type != TYPE_INT || right_type != TYPE_INT)
			{
				semantic_error("mod operator expects integer operands");
			}
			expr->type = TYPE_INT;
			return expr->type;
		case BIN_OP_EQ:
		case BIN_OP_NEQ:
			if (!ensure_assignable(left_type, right_type) && !ensure_assignable(right_type, left_type))
			{
				semantic_error("comparison between incompatible types");
			}
			expr->type = TYPE_BOOL;
			return expr->type;
		case BIN_OP_LT:
		case BIN_OP_LE:
		case BIN_OP_GT:
		case BIN_OP_GE:
			if (!is_numeric(left_type) || !is_numeric(right_type))
			{
				semantic_error("relational operator expects numeric operands");
			}
			expr->type = TYPE_BOOL;
			return expr->type;
		case BIN_OP_AND:
		case BIN_OP_OR:
			if (!is_boolean_like(left_type) || !is_boolean_like(right_type))
			{
				semantic_error("logical operator expects boolean or numeric operands");
			}
			expr->type = TYPE_BOOL;
			return expr->type;
		}
		break;
	}
	case EXPR_CALL:
	{
		TypeKind builtin = analyze_builtin_call(info, symbols, expr);
		if (builtin != TYPE_UNKNOWN)
		{
			expr->type = builtin;
			return expr->type;
		}
		const FunctionSignature *signature = function_table_find(&info->functions, expr->data.call.callee);
		if (!signature)
		{
			semantic_error("call to unknown function '%s'", expr->data.call.callee);
			expr->type = TYPE_UNKNOWN;
			return expr->type;
		}
		if (expr->data.call.args.count != signature->params.count)
		{
			semantic_error("function '%s' expects %zu arguments but got %zu",
						   signature->name,
						   signature->params.count,
						   expr->data.call.args.count);
		}
		size_t limit = expr->data.call.args.count < signature->params.count ? expr->data.call.args.count : signature->params.count;
		for (size_t i = 0; i < limit; ++i)
		{
			AstExpr *arg = expr->data.call.args.items[i];
			TypeKind arg_type = analyze_expression(info, symbols, arg);
			arg->type = arg_type;
			TypeKind expected = signature->params.items[i].type;
			if (!ensure_assignable(expected, arg_type))
			{
				semantic_error("argument %zu of function '%s' expected %s but got %s",
							   i + 1,
							   signature->name,
							   ast_type_name(expected),
							   ast_type_name(arg_type));
				return TYPE_UNKNOWN;
			}
		}
		expr->type = signature->return_type;
		return expr->type;
	}
	}

	expr->type = TYPE_UNKNOWN;
	return expr->type;
}

static int ensure_assignable(TypeKind target, TypeKind value)
{
	if (target == TYPE_UNKNOWN || value == TYPE_UNKNOWN)
	{
		return 1;
	}
	if (target == value)
	{
		return 1;
	}
	if (target == TYPE_ARRAY || value == TYPE_ARRAY)
	{
		return target == TYPE_ARRAY && value == TYPE_ARRAY;
	}
	if (target == TYPE_STRING || value == TYPE_STRING)
	{
		return target == TYPE_STRING && value == TYPE_STRING;
	}
	if (target == TYPE_BOOL && (is_numeric(value) || value == TYPE_BOOL))
	{
		return 1;
	}
	if (is_numeric(target) && value == TYPE_BOOL)
	{
		return 1;
	}
	if (is_numeric(target) && is_numeric(value))
	{
		return 1;
	}
	if (target == TYPE_VOID || value == TYPE_VOID)
	{
		return target == value;
	}
	return 0;
}

static int is_numeric(TypeKind type)
{
	return type == TYPE_INT || type == TYPE_FLOAT || type == TYPE_CHAR;
}

static int is_boolean_like(TypeKind type)
{
	return type == TYPE_BOOL || type == TYPE_INT || type == TYPE_FLOAT || type == TYPE_CHAR;
}

static TypeKind arithmetic_result(TypeKind left, TypeKind right)
{
	if (left == TYPE_FLOAT || right == TYPE_FLOAT)
	{
		return TYPE_FLOAT;
	}
	return TYPE_INT;
}

static TypeKind analyze_builtin_call(SemanticInfo *info, SymbolTable *symbols, AstExpr *expr)
{
	if (!expr || expr->kind != EXPR_CALL)
	{
		return TYPE_UNKNOWN;
	}
	const char *callee = expr->data.call.callee;
	if (strcmp(callee, "printf") == 0)
	{
		if (expr->data.call.args.count == 0)
		{
			semantic_error("printf expects at least one argument");
			return TYPE_UNKNOWN;
		}
		AstExpr *format = expr->data.call.args.items[0];
		TypeKind format_type = analyze_expression(info, symbols, format);
		format->type = format_type;
		if (format_type != TYPE_STRING)
		{
			semantic_error("printf format argument must be string");
		}
		for (size_t i = 1; i < expr->data.call.args.count; ++i)
		{
			AstExpr *arg = expr->data.call.args.items[i];
			TypeKind arg_type = analyze_expression(info, symbols, arg);
			arg->type = arg_type;
		}
		return TYPE_INT;
	}
	if (strcmp(callee, "puts") == 0)
	{
		if (expr->data.call.args.count != 1)
		{
			semantic_error("puts expects exactly one argument");
			return TYPE_UNKNOWN;
		}
		AstExpr *arg = expr->data.call.args.items[0];
		TypeKind arg_type = analyze_expression(info, symbols, arg);
		arg->type = arg_type;
		if (arg_type != TYPE_STRING)
		{
			semantic_error("puts argument must be string");
		}
		return TYPE_INT;
	}
	return TYPE_UNKNOWN;
}
