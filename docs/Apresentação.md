## Introdução (João)

- Motivação: facilitar correções em tempo de execução migrando C educacional para Lua.
- Objetivo: demonstrar pipeline completa (léxico → código Lua) com subconjunto controlado de C.
- Escopo: sem ponteiros/structs; foco em tipos primitivos, controle de fluxo e funções simples.

## Léxico (João)

- Categoria de tokens: palavras-chave, identificadores, números, strings, operadores e delimitadores.
- Estratégia: estados do Flex para distinguir comentários, literais e tratamento de whitespace.
- Símbolos especiais: rastreamento de linha/coluna para diagnósticos.

## Sintático (Sophia)

- Gramática LL(1)/LR: regras Bison para declarações, expressões e blocos.
- Resolução de conflitos: preferência por shift para "dangling else" documentada.
- Produção da AST: ações semânticas instanciam nós e anexam comentários de posição.

## Intermediário (Marco)

- AST: nós para expressões, statements, blocos e funções.
- Tabela de símbolos hierárquica: pilha de escopos para variáveis/arrays/funções.
- Vínculo entre symbol e AST: cada símbolo mantém ponteiro para declaração para otimizações.

## Semântico (Lívia)

- Verificação de tipos: coercões permitidas int↔float/char; proíbe aritmética com string/bool.
- Regras de uso: variáveis precisam ser declaradas antes, arrays apenas com índice inteiro.
- Funções: assinatura registrada, verifica retorno obrigatório e número/tipo de argumentos.

## Geração de código Lua (André)

- Blocos C → "do ... end" preservando escopo.
- Variáveis em C ↔ `local` Lua; arrays como tabelas sequenciais.
- `printf/puts` convertidos para `print` com `string.format`; laços `for` traduzidos para `while`.

## Ambiguidades

- Compiladores precisam estar 100% corretos sobre a estrutura do código fonte.
- Ao mesmo tempo, eles não podem demandar mais contexto do que o necessário para entregar um resultado.
- Léxico: Array vs. operador de indexação...
- Sintático: Dangling else...
- Semântico: Tipagem fraca (Lua) vs. tipagem forte (C)...
- Semântico adicional: comportamento de `bool` em Lua (0 é truthy) vs. C (0 falsy) tratado ao gerar código.

## Otimizações (João)

- Propagação de constantes e simplificações triviais.
- Common Subexpression Elimination insere temporários quando ganho é positivo.
- Dead Code Elimination remove declarações não usadas e statements após `return` mantendo efeitos colaterais.
- Avaliação de condições em tempo de compilação elimina ramos inatingíveis.
