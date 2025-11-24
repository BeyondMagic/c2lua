%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"
#include "lexer.h"

int yy_line = 1;
int yy_column = 1;
int yy_token_line = 1;
int yy_token_column = 1;
static const char *yy_source_name = "<stdin>";

void lexer_set_source_name(const char *name)
{
    yy_source_name = (name && *name) ? name : "<stdin>";
}

const char *lexer_get_source_name(void)
{
    return yy_source_name ? yy_source_name : "<stdin>";
}

void lexer_reset_position(void)
{
    yy_line = 1;
    yy_column = 1;
    yy_token_line = 1;
    yy_token_column = 1;
}

#define YY_USER_ACTION                                                     \
    {                                                                       \
        yy_token_line = yy_line;                                              \
        yy_token_column = yy_column;                                          \
        for (int yy_i = 0; yy_i < yyleng; ++yy_i)                             \
        {                                                                   \
            if (yytext[yy_i] == '\n')                                        \
            {                                                               \
                yy_line++;                                                    \
                yy_column = 1;                                                \
            }                                                               \
            else                                                            \
            {                                                               \
                yy_column++;                                                  \
            }                                                               \
        }                                                                   \
    }

static char *yy_strdup(const char *src)
{
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

static char *yy_parse_string_literal(const char *src)
{
    size_t len = strlen(src);
    char *buffer = malloc(len);
    if (!buffer)
    {
        fprintf(stderr, "out of memory\n");
        exit(EXIT_FAILURE);
    }
    size_t out = 0;
    for (size_t i = 1; i + 1 < len; ++i)
    {
        char c = src[i];
        if (c == '\\' && i + 1 < len)
        {
            char next = src[++i];
            switch (next)
            {
            case 'n':
                buffer[out++] = '\n';
                break;
            case 't':
                buffer[out++] = '\t';
                break;
            case '\\':
                buffer[out++] = '\\';
                break;
            case '"':
                buffer[out++] = '"';
                break;
            default:
                buffer[out++] = next;
                break;
            }
        }
        else
        {
            buffer[out++] = c;
        }
    }
    buffer[out] = '\0';
    return buffer;
}

static long long yy_parse_char_literal(const char *src)
{
    size_t len = strlen(src);
    if (len < 3 || src[0] != '\'' || src[len - 1] != '\'')
    {
        return 0;
    }
    if (src[1] != '\\')
    {
        return (unsigned char)src[1];
    }
    if (len < 4)
    {
        return 0;
    }
    char esc = src[2];
    switch (esc)
    {
    case '\\':
        return '\\';
    case '\'':
        return '\'';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case '0':
        return '\0';
    case '"':
        return '"';
    default:
        return (unsigned char)esc;
    }
}
%}

%option noyywrap nodefault noinput nounput

%x COMMENT

%%

[ \t\r\n]+                         { /* skip whitespace */ }
"int"                              { return KW_INT; }
"char"                             { return KW_CHAR; }
"float"                            { return KW_FLOAT; }
"bool"                             { return KW_BOOL; }
"void"                             { return KW_VOID; }
"return"                           { return RETURN; }
"while"                            { return WHILE; }
"for"                              { return FOR; }
"true"                             { return TRUE; }
"false"                            { return FALSE; }
"\["                               { return LBRACKET; }
"\]"                               { return RBRACKET; }
"//"[^\n]*                         { /* skip single line comments */ }
"/*"                { BEGIN(COMMENT); }
<COMMENT>{
    "*/"            { BEGIN(INITIAL); }
    [^*\n]+         { /* Eat any character that's not a '*' or a newline */ }
    "*"             { /* Eat a '*' */ }
    \n              { /* Eat a newline */ }
    <<EOF>>         { 
                        fprintf(stderr, "Error: Unterminated block comment.\n"); 
                        yyterminate(); 
                    }
}
[0-9]+"."[0-9]*([eE][-+]?[0-9]+)?  { yylval.floatValue = strtod(yytext, NULL); return FLOAT_LITERAL; }
[0-9]+                             { yylval.intValue = strtoll(yytext, NULL, 10); return INT_LITERAL; }
[a-zA-Z_][a-zA-Z0-9_]*             { yylval.id = yy_strdup(yytext); return IDENT; }
"=="                               { return EQ; }
"!="                               { return NEQ; }
"&&"                               { return AND; }
"||"                               { return OR; }
"<="                               { return LE; }
">="                               { return GE; }
"<"                                { return LT; }
">"                                { return GT; }
"+"                                { return PLUS; }
"-"                                { return MINUS; }
"*"                                { return TIMES; }
"/"                                { return DIVIDE; }
"%"                                { return MOD; }
"!"                                { return NOT; }
"="                                { return ASSIGN; }
","                                { return COMMA; }
";"                                { return SEMI; }
"("                                { return LPAREN; }
")"                                { return RPAREN; }
"{"                                { return LBRACE; }
"}"                                { return RBRACE; }
\"([^\"\n]|\\.)*\"             { yylval.string = yy_parse_string_literal(yytext); return STRING_LITERAL; }
'([^\\'\n]|\\.)'             { yylval.intValue = yy_parse_char_literal(yytext); return CHAR_LITERAL; }
.                                  { fprintf(stderr, "%s:%d:%d: invalid character '%s'\n", lexer_get_source_name(), yy_token_line, yy_token_column, yytext); }

%%