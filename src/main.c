#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "codegen_lua.h"
#include "lexer.h"
#include "optimizer.h"
#include "semantic.h"
#include "parser.tab.h"

static FILE *open_input(int argc, char **argv);

int main(int argc, char **argv)
{
	FILE *input = open_input(argc, argv);
	if (!input)
	{
		return EXIT_FAILURE;
	}

	const char *source_name = (argc == 2) ? argv[1] : "<stdin>";
	lexer_set_source_name(source_name);
	lexer_reset_position();

	AstProgram *program = c2lua_parse(input);
	if (!program)
	{
		if (input != stdin)
		{
			fclose(input);
		}
		return EXIT_FAILURE;
	}

	SemanticInfo sem_info;
	if (!semantic_analyze(program, &sem_info))
	{
		ast_program_destroy(program);
		if (input != stdin)
		{
			fclose(input);
		}
		return EXIT_FAILURE;
	}

	optimize_program(program);

	codegen_lua_emit(stdout, program, &sem_info.functions);

	semantic_info_free(&sem_info);
	ast_program_destroy(program);

	if (input != stdin)
	{
		fclose(input);
	}

	return EXIT_SUCCESS;
}

static FILE *open_input(int argc, char **argv)
{
	if (argc > 2)
	{
		fprintf(stderr, "Usage: %s [input.c]\n", argv[0]);
		return NULL;
	}

	if (argc == 2)
	{
		FILE *file = fopen(argv[1], "r");
		if (!file)
		{
			fprintf(stderr, "failed to open '%s': %s\n", argv[1], strerror(errno));
			return NULL;
		}
		return file;
	}

	return stdin;
}
