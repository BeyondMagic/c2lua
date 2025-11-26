#include "optimizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	AstExpr **location;
	size_t stmt_index;
	AstStmt *stmt;
} CseOccurrence;

typedef struct
{
	char *key;
	TypeKind type;
	size_t first_stmt_index;
	CseOccurrence *occurrences;
	size_t occurrence_count;
	size_t occurrence_capacity;
} CseEntry;

typedef struct
{
	CseEntry *items;
	size_t count;
	size_t capacity;
} CseEntryList;

static void optimize_function(AstFunction *fn);
static void optimize_block(AstBlock *block);
static void optimize_statement_children(AstStmt *stmt);
static void collect_block_candidates(AstBlock *block, CseEntryList *entries);
static void collect_statement_candidates(AstStmt *stmt, size_t stmt_index, CseEntryList *entries);
static void collect_expr_candidates(AstExpr **expr_ptr, size_t stmt_index, AstStmt *owner, CseEntryList *entries);
static int expr_is_constant(const AstExpr *expr);
static int expr_is_candidate(const AstExpr *expr);
static char *expr_make_key(const AstExpr *expr);
static void register_candidate(CseEntryList *entries, AstExpr **expr_ptr, size_t stmt_index, AstStmt *owner);
static CseEntry *find_entry(CseEntryList *entries, const char *key);
static void ensure_entry_capacity(CseEntryList *list, size_t needed);
static void ensure_occ_capacity(CseEntry *entry, size_t needed);
static void add_occurrence(CseEntry *entry, AstExpr **location, size_t stmt_index, AstStmt *stmt);
static void apply_cse(AstBlock *block, CseEntryList *entries);
static void insert_statement(AstStmtList *list, size_t index, AstStmt *stmt);
static AstExpr *make_temp_identifier(const char *name, TypeKind type);
static char *make_temp_name(void);
static void free_entries(CseEntryList *entries);
static char *dup_string(const char *value);
static void *xmalloc(size_t size);
static int compare_entries_by_index(const void *lhs, const void *rhs);
static int occurrence_survives_dce(const CseOccurrence *occ);
static void eliminate_unreachable(AstStmtList *list);

void optimize_program(AstProgram *program)
{
	if (!program)
	{
		return;
	}
	for (size_t i = 0; i < program->functions.count; ++i)
	{
		optimize_function(program->functions.items[i]);
	}
}

static void optimize_function(AstFunction *fn)
{
	if (!fn)
	{
		return;
	}
	optimize_block(&fn->body);
}

static void optimize_block(AstBlock *block)
{
	if (!block)
	{
		return;
	}

	CseEntryList entries = {0};
	collect_block_candidates(block, &entries);
	apply_cse(block, &entries);
	free_entries(&entries);

	for (size_t i = 0; i < block->statements.count; ++i)
	{
		optimize_statement_children(block->statements.items[i]);
	}
	eliminate_unreachable(&block->statements);
}

static void optimize_statement_children(AstStmt *stmt)
{
	if (!stmt)
	{
		return;
	}

	switch (stmt->kind)
	{
	case STMT_BLOCK:
		optimize_block(&stmt->data.block);
		break;
	case STMT_WHILE:
		optimize_statement_children(stmt->data.while_stmt.body);
		break;
	case STMT_FOR:
		optimize_statement_children(stmt->data.for_stmt.init);
		optimize_statement_children(stmt->data.for_stmt.body);
		optimize_statement_children(stmt->data.for_stmt.post);
		break;
	default:
		break;
	}
}

static void collect_block_candidates(AstBlock *block, CseEntryList *entries)
{
	if (!block)
	{
		return;
	}
	for (size_t i = 0; i < block->statements.count; ++i)
	{
		collect_statement_candidates(block->statements.items[i], i, entries);
	}
}

static void collect_statement_candidates(AstStmt *stmt, size_t stmt_index, CseEntryList *entries)
{
	if (!stmt)
	{
		return;
	}

	switch (stmt->kind)
	{
	case STMT_DECL:
		if (!stmt->data.decl.is_array && stmt->data.decl.init)
		{
			collect_expr_candidates(&stmt->data.decl.init, stmt_index, stmt, entries);
		}
		break;
	case STMT_ASSIGN:
		collect_expr_candidates(&stmt->data.assign.value, stmt_index, stmt, entries);
		break;
	case STMT_ARRAY_ASSIGN:
		collect_expr_candidates(&stmt->data.array_assign.value, stmt_index, stmt, entries);
		break;
	case STMT_EXPR:
	case STMT_RETURN:
		if (stmt->data.expr)
		{
			collect_expr_candidates(&stmt->data.expr, stmt_index, stmt, entries);
		}
		break;
	case STMT_WHILE:
	case STMT_FOR:
		/* Loop conditions must be evaluated every iteration, so skip them. */
		break;
	case STMT_BLOCK:
	default:
		break;
	}
}

static void collect_expr_candidates(AstExpr **expr_ptr, size_t stmt_index, AstStmt *owner, CseEntryList *entries)
{
	if (!expr_ptr || !*expr_ptr)
	{
		return;
	}

	AstExpr *expr = *expr_ptr;

	switch (expr->kind)
	{
	case EXPR_BINARY:
		collect_expr_candidates(&expr->data.binary.left, stmt_index, owner, entries);
		collect_expr_candidates(&expr->data.binary.right, stmt_index, owner, entries);
		break;
	case EXPR_UNARY:
		collect_expr_candidates(&expr->data.unary.operand, stmt_index, owner, entries);
		break;
	case EXPR_CALL:
		for (size_t i = 0; i < expr->data.call.args.count; ++i)
		{
			collect_expr_candidates(&expr->data.call.args.items[i], stmt_index, owner, entries);
		}
		break;
	case EXPR_ARRAY_LITERAL:
		for (size_t i = 0; i < expr->data.array_literal.elements.count; ++i)
		{
			collect_expr_candidates(&expr->data.array_literal.elements.items[i], stmt_index, owner, entries);
		}
		break;
	case EXPR_SUBSCRIPT:
		collect_expr_candidates(&expr->data.subscript.array, stmt_index, owner, entries);
		collect_expr_candidates(&expr->data.subscript.index, stmt_index, owner, entries);
		break;
	default:
		break;
	}

	if (expr_is_candidate(expr))
	{
		register_candidate(entries, expr_ptr, stmt_index, owner);
	}
}

static int expr_is_constant(const AstExpr *expr)
{
	if (!expr)
	{
		return 0;
	}

	switch (expr->kind)
	{
	case EXPR_INT_LITERAL:
	case EXPR_FLOAT_LITERAL:
	case EXPR_BOOL_LITERAL:
	case EXPR_STRING_LITERAL:
		return 1;
	case EXPR_UNARY:
		return expr_is_constant(expr->data.unary.operand);
	case EXPR_BINARY:
		return expr_is_constant(expr->data.binary.left) && expr_is_constant(expr->data.binary.right);
	default:
		return 0;
	}
}

static int expr_is_candidate(const AstExpr *expr)
{
	if (!expr)
	{
		return 0;
	}
	if ((expr->kind != EXPR_BINARY && expr->kind != EXPR_UNARY) || expr->type == TYPE_UNKNOWN)
	{
		return 0;
	}
	return expr_is_constant(expr);
}

static void register_candidate(CseEntryList *entries, AstExpr **expr_ptr, size_t stmt_index, AstStmt *owner)
{
	if (!entries || !expr_ptr || !*expr_ptr)
	{
		return;
	}

	AstExpr *expr = *expr_ptr;
	char *key = expr_make_key(expr);
	CseEntry *entry = find_entry(entries, key);
	if (!entry)
	{
		ensure_entry_capacity(entries, entries->count + 1);
		entry = &entries->items[entries->count++];
		entry->key = key;
		entry->type = expr->type;
		entry->first_stmt_index = stmt_index;
		entry->occurrences = NULL;
		entry->occurrence_count = 0;
		entry->occurrence_capacity = 0;
	}
	else
	{
		free(key);
		if (stmt_index < entry->first_stmt_index)
		{
			entry->first_stmt_index = stmt_index;
		}
	}
	add_occurrence(entry, expr_ptr, stmt_index, owner);
}

static CseEntry *find_entry(CseEntryList *entries, const char *key)
{
	if (!entries || !key)
	{
		return NULL;
	}
	for (size_t i = 0; i < entries->count; ++i)
	{
		if (strcmp(entries->items[i].key, key) == 0)
		{
			return &entries->items[i];
		}
	}
	return NULL;
}

static void ensure_entry_capacity(CseEntryList *list, size_t needed)
{
	if (!list)
	{
		return;
	}
	if (list->capacity >= needed)
	{
		return;
	}
	size_t new_capacity = list->capacity ? list->capacity * 2 : 4;
	while (new_capacity < needed)
	{
		new_capacity *= 2;
	}
	CseEntry *new_items = realloc(list->items, new_capacity * sizeof(CseEntry));
	if (!new_items)
	{
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	list->items = new_items;
	list->capacity = new_capacity;
}

static void ensure_occ_capacity(CseEntry *entry, size_t needed)
{
	if (!entry)
	{
		return;
	}
	if (entry->occurrence_capacity >= needed)
	{
		return;
	}
	size_t new_capacity = entry->occurrence_capacity ? entry->occurrence_capacity * 2 : 4;
	while (new_capacity < needed)
	{
		new_capacity *= 2;
	}
	CseOccurrence *new_items = realloc(entry->occurrences, new_capacity * sizeof(CseOccurrence));
	if (!new_items)
	{
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	entry->occurrences = new_items;
	entry->occurrence_capacity = new_capacity;
}

static void add_occurrence(CseEntry *entry, AstExpr **location, size_t stmt_index, AstStmt *stmt)
{
	if (!entry)
	{
		return;
	}
	ensure_occ_capacity(entry, entry->occurrence_count + 1);
	entry->occurrences[entry->occurrence_count].location = location;
	entry->occurrences[entry->occurrence_count].stmt_index = stmt_index;
	entry->occurrences[entry->occurrence_count].stmt = stmt;
	entry->occurrence_count++;
}

static void apply_cse(AstBlock *block, CseEntryList *entries)
{
	if (!block || !entries || entries->count == 0)
	{
		return;
	}

	size_t reusable = 0;
	for (size_t i = 0; i < entries->count; ++i)
	{
		if (entries->items[i].occurrence_count >= 2)
		{
			reusable++;
		}
	}
	if (reusable == 0)
	{
		return;
	}

	CseEntry **selected = xmalloc(sizeof(CseEntry *) * reusable);
	size_t idx = 0;
	for (size_t i = 0; i < entries->count; ++i)
	{
		if (entries->items[i].occurrence_count >= 2)
		{
			selected[idx++] = &entries->items[i];
		}
	}
	qsort(selected, reusable, sizeof(CseEntry *), compare_entries_by_index);

	size_t inserted = 0;
	for (size_t i = 0; i < reusable; ++i)
	{
		CseEntry *entry = selected[i];
		if (!entry || entry->occurrence_count < 2)
		{
			continue;
		}
		AstExpr **first_slot = entry->occurrences[0].location;
		if (!first_slot || !*first_slot)
		{
			continue;
		}
		AstExpr *init_expr = *first_slot;
		char *temp_name = make_temp_name();

		*first_slot = make_temp_identifier(temp_name, entry->type);
		for (size_t j = 1; j < entry->occurrence_count; ++j)
		{
			AstExpr **slot = entry->occurrences[j].location;
			if (slot)
			{
				*slot = make_temp_identifier(temp_name, entry->type);
			}
		}

		int has_live_use = 0;
		for (size_t j = 0; j < entry->occurrence_count; ++j)
		{
			if (occurrence_survives_dce(&entry->occurrences[j]))
			{
				has_live_use = 1;
				break;
			}
		}

		AstStmt *decl = ast_stmt_make_decl(entry->type, dup_string(temp_name), init_expr);
		if (has_live_use)
		{
			decl->data.decl.is_used = 1; /* mark as live so DCE keeps this temp */
		}
		insert_statement(&block->statements, entry->first_stmt_index + inserted, decl);
		inserted++;
		free(temp_name);
	}

	free(selected);
}

static void insert_statement(AstStmtList *list, size_t index, AstStmt *stmt)
{
	if (!list || !stmt)
	{
		return;
	}
	if (index > list->count)
	{
		index = list->count;
	}
	if (list->capacity <= list->count)
	{
		size_t new_capacity = list->capacity ? list->capacity * 2 : 4;
		AstStmt **new_items = realloc(list->items, new_capacity * sizeof(AstStmt *));
		if (!new_items)
		{
			fprintf(stderr, "out of memory\n");
			exit(EXIT_FAILURE);
		}
		list->items = new_items;
		list->capacity = new_capacity;
	}
	if (index < list->count)
	{
		memmove(&list->items[index + 1], &list->items[index], (list->count - index) * sizeof(AstStmt *));
	}
	list->items[index] = stmt;
	list->count++;
}

static AstExpr *make_temp_identifier(const char *name, TypeKind type)
{
	AstExpr *identifier = ast_expr_make_identifier(dup_string(name));
	identifier->type = type;
	return identifier;
}

static char *make_temp_name(void)
{
	static size_t counter = 0;
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "__c2lua_cse_%zu", counter++);
	return dup_string(buffer);
}

static void free_entries(CseEntryList *entries)
{
	if (!entries)
	{
		return;
	}
	for (size_t i = 0; i < entries->count; ++i)
	{
		free(entries->items[i].key);
		free(entries->items[i].occurrences);
	}
	free(entries->items);
	entries->items = NULL;
	entries->count = 0;
	entries->capacity = 0;
}

static char *dup_string(const char *value)
{
	const char *src = value ? value : "";
	char *copy = strdup(src);
	if (!copy)
	{
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	return copy;
}

static void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr)
	{
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	return ptr;
}

static int compare_entries_by_index(const void *lhs, const void *rhs)
{
	const CseEntry *const *a = lhs;
	const CseEntry *const *b = rhs;
	if ((*a)->first_stmt_index < (*b)->first_stmt_index)
	{
		return -1;
	}
	if ((*a)->first_stmt_index > (*b)->first_stmt_index)
	{
		return 1;
	}
	return 0;
}

static int occurrence_survives_dce(const CseOccurrence *occ)
{
	if (!occ || !occ->stmt)
	{
		return 1;
	}
	if (occ->stmt->kind != STMT_DECL)
	{
		return 1;
	}
	return occ->stmt->data.decl.is_used != 0;
}

static char *expr_make_key(const AstExpr *expr)
{
	if (!expr)
	{
		return dup_string("<nil>");
	}
	char buffer[64];
	switch (expr->kind)
	{
	case EXPR_INT_LITERAL:
		// Include type information in the key to distinguish between int and char literals
		if (expr->type == TYPE_CHAR)
		{
			snprintf(buffer, sizeof(buffer), "C:%lld", expr->data.int_value);
		}
		else
		{
			snprintf(buffer, sizeof(buffer), "I:%lld", expr->data.int_value);
		}
		return dup_string(buffer);
	case EXPR_FLOAT_LITERAL:
		snprintf(buffer, sizeof(buffer), "F:%g", expr->data.float_value);
		return dup_string(buffer);
	case EXPR_BOOL_LITERAL:
		snprintf(buffer, sizeof(buffer), "B:%d", expr->data.bool_value != 0);
		return dup_string(buffer);
	case EXPR_STRING_LITERAL:
	{
		size_t len = expr->data.string_literal ? strlen(expr->data.string_literal) : 0;
		size_t total = len + 32;
		char *result = xmalloc(total);
		snprintf(result, total, "S:%zu:%s", len, expr->data.string_literal ? expr->data.string_literal : "");
		return result;
	}
	case EXPR_UNARY:
	{
		char *operand = expr_make_key(expr->data.unary.operand);
		size_t total = strlen(operand) + 32;
		char *result = xmalloc(total);
		snprintf(result, total, "U:%d:%s", expr->data.unary.op, operand);
		free(operand);
		return result;
	}
	case EXPR_BINARY:
	{
		char *left = expr_make_key(expr->data.binary.left);
		char *right = expr_make_key(expr->data.binary.right);
		size_t total = strlen(left) + strlen(right) + 64;
		char *result = xmalloc(total);
		snprintf(result, total, "BIN:%d:%s|%s", expr->data.binary.op, left, right);
		free(left);
		free(right);
		return result;
	}
	default:
		return dup_string("<unsupported>");
	}
}

static void eliminate_unreachable(AstStmtList *list) {
    if (!list) return;

    int after_return = 0;
    size_t write = 0;

    for (size_t i = 0; i < list->count; i++) {
        AstStmt *s = list->items[i];
        if (after_return) {
            continue;
        }
        list->items[write++] = s;
        if (s->kind == STMT_RETURN) {
            after_return = 1;
        }
    }

    list->count = write;
}

