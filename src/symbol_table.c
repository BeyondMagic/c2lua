#include "symbol_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_string(const char *src)
{
	if (!src)
	{
		return NULL;
	}
	size_t len = strlen(src) + 1;
	char *copy = malloc(len);
	if (!copy)
	{
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}
	memcpy(copy, src, len);
	return copy;
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

void symbol_table_init(SymbolTable *table)
{
	if (!table)
	{
		return;
	}
	table->scopes = NULL;
	table->depth = 0;
	table->capacity = 0;
}

static void symbol_scope_free(SymbolScope *scope)
{
	if (!scope)
	{
		return;
	}
	for (size_t i = 0; i < scope->count; ++i)
	{
		free(scope->items[i].name);
	}
	free(scope->items);
	scope->items = NULL;
	scope->count = 0;
	scope->capacity = 0;
}

void symbol_table_free(SymbolTable *table)
{
	if (!table)
	{
		return;
	}
	for (size_t i = 0; i < table->depth; ++i)
	{
		symbol_scope_free(&table->scopes[i]);
	}
	free(table->scopes);
	table->scopes = NULL;
	table->depth = 0;
	table->capacity = 0;
}

void symbol_table_push_scope(SymbolTable *table)
{
	if (!table)
	{
		return;
	}
	ensure_capacity((void **)&table->scopes, sizeof(SymbolScope), &table->capacity, table->depth + 1);
	SymbolScope *scope = &table->scopes[table->depth++];
	scope->items = NULL;
	scope->count = 0;
	scope->capacity = 0;
}

void symbol_table_pop_scope(SymbolTable *table)
{
	if (!table || table->depth == 0)
	{
		return;
	}
	SymbolScope *scope = &table->scopes[table->depth - 1];
	symbol_scope_free(scope);
	table->depth--;
}

int symbol_table_add(SymbolTable *table, const char *name, TypeKind type, int is_array, size_t array_size, TypeKind element_type)
{
	if (!table || table->depth == 0 || !name)
	{
		return 0;
	}
	SymbolScope *scope = &table->scopes[table->depth - 1];
	for (size_t i = 0; i < scope->count; ++i)
	{
		if (strcmp(scope->items[i].name, name) == 0)
		{
			return 0;
		}
	}
	ensure_capacity((void **)&scope->items, sizeof(Symbol), &scope->capacity, scope->count + 1);
	scope->items[scope->count].name = dup_string(name);
	scope->items[scope->count].type = type;
	scope->items[scope->count].is_array = is_array ? 1 : 0;
	scope->items[scope->count].array_size = array_size;
	scope->items[scope->count].element_type = element_type;
	scope->items[scope->count].used = 0; /* inicializa como não usado */
	scope->count++;
	return 1;
}

const Symbol *symbol_table_lookup(const SymbolTable *table, const char *name)
{
	if (!table || !name)
	{
		return NULL;
	}
	for (size_t depth = table->depth; depth > 0; --depth)
	{
		const SymbolScope *scope = &table->scopes[depth - 1];
		for (size_t i = 0; i < scope->count; ++i)
		{
			if (strcmp(scope->items[i].name, name) == 0)
			{
				return &scope->items[i];
			}
		}
	}
	return NULL;
}

/* Nova função: marca um símbolo como usado (procura do escopo atual para cima) */
void symbol_table_mark_used(SymbolTable *table, const char *name)
{
	if (!table || !name)
		return;
	for (size_t depth = table->depth; depth > 0; --depth)
	{
		SymbolScope *scope = &table->scopes[depth - 1];
		for (size_t i = 0; i < scope->count; ++i)
		{
			if (strcmp(scope->items[i].name, name) == 0)
			{
				scope->items[i].used = 1;
				return;
			}
		}
	}
}

/* ================= function table (sem alterações) ================== */

void function_table_init(FunctionTable *table)
{
	if (!table)
	{
		return;
	}
	table->items = NULL;
	table->count = 0;
	table->capacity = 0;
}

static void function_signature_free(FunctionSignature *signature)
{
	if (!signature)
	{
		return;
	}
	free(signature->name);
	ast_param_list_destroy(&signature->params);
}

void function_table_free(FunctionTable *table)
{
	if (!table)
	{
		return;
	}
	for (size_t i = 0; i < table->count; ++i)
	{
		function_signature_free(&table->items[i]);
	}
	free(table->items);
	table->items = NULL;
	table->count = 0;
	table->capacity = 0;
}

FunctionSignature *function_table_add(FunctionTable *table, const char *name, TypeKind return_type, const AstParamList *params)
{
	if (!table || !name)
	{
		return NULL;
	}
	for (size_t i = 0; i < table->count; ++i)
	{
		if (strcmp(table->items[i].name, name) == 0)
		{
			return NULL;
		}
	}
	ensure_capacity((void **)&table->items, sizeof(FunctionSignature), &table->capacity, table->count + 1);
	FunctionSignature *signature = &table->items[table->count++];
	signature->name = dup_string(name);
	signature->return_type = return_type;
	signature->params = ast_param_list_make();
	if (params)
	{
		for (size_t i = 0; i < params->count; ++i)
		{
			AstParam param;
			param.type = params->items[i].type;
			param.name = dup_string(params->items[i].name);
			ast_param_list_push(&signature->params, param);
		}
	}
	return signature;
}

const FunctionSignature *function_table_find(const FunctionTable *table, const char *name)
{
	if (!table || !name)
	{
		return NULL;
	}
	for (size_t i = 0; i < table->count; ++i)
	{
		if (strcmp(table->items[i].name, name) == 0)
		{
			return &table->items[i];
		}
	}
	return NULL;
}
