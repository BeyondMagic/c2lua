#ifndef LEXER_H
#define LEXER_H

const char *lexer_get_source_name(void);
void lexer_set_source_name(const char *name);
void lexer_reset_position(void);

extern int yy_line;
extern int yy_column;
extern int yy_token_line;
extern int yy_token_column;

#endif
