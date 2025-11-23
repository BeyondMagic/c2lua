#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "ast.h"

typedef struct
{
	char *name;
	TypeKind type;
	int is_array;
	size_t array_size;
	TypeKind element_type;
	int used; /* novo: 0 = não usado, 1 = usado (marcado durante análise/otimização) */
} Symbol;

typedef struct
{
	Symbol *items;
	size_t count;
	size_t capacity;
} SymbolScope;

typedef struct
{
	SymbolScope *scopes;
	size_t depth;
	size_t capacity;
} SymbolTable;

typedef struct
{
	char *name;
	TypeKind return_type;
	AstParamList params;
} FunctionSignature;

typedef struct
{
	FunctionSignature *items;
	size_t count;
	size_t capacity;
} FunctionTable;

void symbol_table_init(SymbolTable *table);
void symbol_table_free(SymbolTable *table);
void symbol_table_push_scope(SymbolTable *table);
void symbol_table_pop_scope(SymbolTable *table);
int symbol_table_add(SymbolTable *table, const char *name, TypeKind type, int is_array, size_t array_size, TypeKind element_type);
const Symbol *symbol_table_lookup(const SymbolTable *table, const char *name);

/* nova API para marcar um símbolo como "usado" (leitura) */
void symbol_table_mark_used(SymbolTable *table, const char *name);

void function_table_init(FunctionTable *table);
void function_table_free(FunctionTable *table);
FunctionSignature *function_table_add(FunctionTable *table, const char *name, TypeKind return_type, const AstParamList *params);
const FunctionSignature *function_table_find(const FunctionTable *table, const char *name);

#endif
