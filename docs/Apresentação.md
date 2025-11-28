## Introdução

- Motivação: facilitar correções em tempo de execução migrando C educacional para Lua.
- Objetivo: demonstrar pipeline completa (léxico → código Lua) com subconjunto controlado de C.
- Escopo: sem ponteiros/structs; foco em tipos primitivos, controle de fluxo e funções simples.

## Léxico

- `src/lexer.l`: Categoria de tokens: palavras-chave, identificadores, números, strings, operadores e delimitadores.
- `src/lexer.l`: Estratégia de estados do Flex para distinguir comentários, literais e tratamento de whitespace.
- `src/lexer.l`: Símbolos especiais com rastreamento de linha/coluna para diagnósticos.

## Sintático

- `src/parser.y`: Gramática LR do Bison para declarações, expressões e blocos.
- `src/parser.y`: Resolução de conflitos com preferência por shift para "dangling else" documentada.
- `src/parser.y`: Ações semânticas que instanciam nós AST e anotam posição.

## Intermediário

- `src/ast.c`/`src/ast.h`: Nós para expressões, statements, blocos e funções.
- `src/symbol_table.c`/`src/symbol_table.h`: Tabela de símbolos hierárquica com pilha de escopos para variáveis/arrays/funções.
- `src/symbol_table.c`: Vínculo entre símbolo e AST guardando ponteiro para a declaração.

## Semântico

- `src/semantic.c`: Verificação de tipos (coerções int↔float/char, bloqueio de string/bool em aritmética).
- `src/semantic.c`: Regras de uso (declaração prévia, índice inteiro para arrays).
- `src/semantic.c`: Controle de funções (assinaturas, retorno obrigatório, número/tipo de argumentos).

## Geração de código Lua

- `src/codegen_lua.c`: Blocos C → "do ... end" preservando escopo.
- `src/codegen_lua.c`: Variáveis `local`, arrays como tabelas sequenciais.
- `src/codegen_lua.c`: `printf/puts` convertidos para `print` com `string.format`; laços `for` traduzidos para `while`.

## Ambiguidades

- Compiladores precisam estar 100% corretos sobre a estrutura do código fonte.
- Ao mesmo tempo, eles não podem demandar mais contexto do que o necessário para entregar um resultado.
- `src/lexer.l`: Array vs. operador de indexação (ex.: `a[10]` vs. `a [ 10 ]`).
- `src/parser.y`: Dangling else – associar o `else` ao `if` mais recente.
- `src/semantic.c`: Tipagem fraca (Lua) vs. forte (C).
- `src/codegen_lua.c`: Conversão de booleanos (0 truthy em Lua, falsy em C).

## Otimizações

- `src/optimizer.c`: Propagação de constantes e simplificações triviais.
- `src/optimizer.c`: Common Subexpression Elimination insere temporários quando há ganho.
- `src/optimizer.c`: Dead Code Elimination remove declarações não usadas e statements após `return` preservando efeitos colaterais.
- `src/optimizer.c`: Avaliação de condições em tempo de compilação elimina ramos inatingíveis.
