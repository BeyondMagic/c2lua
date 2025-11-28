# Projeto: Transpilador de C para Lua (Flex + Bison + C)

Objetivo: construir um compilador que lê um subconjunto de C e transpila para Lua, usando Flex (léxico) e Bison (sintático) em C.

Resumo das funcionalidades suportadas:
- Tipos: int, float, bool (true/false), char (como número), string literal (printf e puts), array unidimensional dos tipos primitivos anteriores.
- Estruturas: declaração de variáveis, atribuição, expressões aritméticas/lógicas, if/else, while, for (apenas uma declaração de variável), função (definição/chamada) e return.
- I/O: mapeamento simples printf/puts -> print com formatação;
- Saída Lua: usar local para variáveis, funções Lua equivalentes, operadores com mesma semântica; arrays opcionais como tabelas.
- Extra: otimizações simples (constantes, dead code, reutilização de subexpressão comum, condição falsa/verdadeira em tempo de compilação).

O que não será suportado:
- Ponteiros (incluindo string), structs, alocação dinâmica, manipulação de memória.
- Recursos avançados: pré-processador, macros, diretivas de compilação.
- Recursos complexos: manipulação de bits, operações em baixo nível, inline assembly.

## Arquitetura

O compilador é dividido em várias fases principais:
1. **Análise Léxica**: Utiliza Flex para tokenizar o código-fonte em C.
2. **Análise Sintática**: Utiliza Bison para enviar à fase seguinte uma árvore sintática.
3. **Intermediária**: Representação da AST (Árvore de Sintaxe Abstrata) para facilitar a análise semântica e otimizações.
4. **Análise Semântica**: Verifica tipos, escopos e outras regras semânticas.
5. **Otimizações**: Aplica otimizações simples na AST.
6. **Geração de Código**: Transpila a AST otimizada para código Lua.

## Estrutura do Projeto

```
.
├── src/                     # Implementação do compilador propriamente dita
│   ├── lexer.*              # Regras Flex e helpers de tokenização
│   ├── parser.y             # Gramática Bison que constrói a AST
│   ├── ast.*                # Tipos da AST e utilitários de construção/cleanup
│   ├── symbol_table.*       # Estruturas de escopo usadas na análise semântica
│   ├── semantic.*           # Verificações de tipo/uso que anotam a AST
│   ├── optimizer.*          # Passes locais (CSE, DCE, eliminação de código morto)
│   ├── codegen_lua.*        # Emissão de Lua a partir da AST anotada
│   └── main.c               # Entrada do programa: orquestra todas as fases
├── tests/
│   ├── pass/                # Pares C/Lua que devem equivaler
│   └── fail/                # Exemplos que precisam produzir erro com .err esperado
└── Makefile                 # Alvo único que compila/roda suite de testes
```

## Ambiente de Desenvolvimento

Ubuntu Linux:
```bash
sudo apt update
sudo apt install -y flex bison build-essential git
```

Arch Linux:
```bash
sudo pacman -Syu flex bison base-devel git
```

Build local (exemplo mínimo):
```bash
make 
./c2lua tests/pass/expressions.c
```

# Documentação de cada sprint

- [1ª sprint](./docs/sprints/1.md);
- [2ª sprint](./docs/sprints/2.md);
- [3ª sprint](./docs/sprints/3.md);
- [4ª sprint](./docs/sprints/4.md);

# Testes

A pasta `/tests/` está organizada em:
- `/tests/pass/`: códigos em C e seus respectivos códigos Lua esperados;
- `/tests/fail/`: códigos em C que devem gerar erros, com seus respectivos arquivos `.err` contendo mensagens esperadas;

Para rodar os testes, utilize o comando:

```bash
make test
```

## Cobertura

Válido (deve passar):
- `arrays.c`: criação, atribuição e leitura em arrays unidimensionais.
- `arith.c`: operações aritméticas básicas e precedência.
- `effect.c`: garante que efeitos colaterais em chamadas/expressões sejam preservados.
- `dead_variable.c`: declarações inutilizadas são removidas sem afetar o resto.
- `char.c`: manipulação de caracteres tratados como inteiros.
- `logic.c`: operadores lógicos e conversões implícitas para booleano.
- `printf.c`: mapeamento de `printf`/`puts` para `print` com formatação.
- `cse.c`: reutilização de subexpressões constantes via temporários.
- `if.c`: condicionais simples.
- `while.c`: laços `while` com updates no corpo.
- `variable.c`: declarações/atribuições básicas com tipos primitivos.
- `deadcode.c`: remoção de código após `return`.
- `for.c`: laço `for` com init/condição/post traduzido para `while` Lua.
- `expressions.c`: combinações de chamadas de função e expressões compostas.

Inválido (deve falhar):
- `bad_if.c`: `if` com condição inválida sinaliza erro sintático/semântico.
- `wrong_assign.c`: incompatibilidade de tipos em atribuições.
- `array_index_string.c`: índice de array usando string gera erro de tipo.
- `int_plus_string.c`: soma entre inteiro e string não é permitida.
- `scope_error.c`: detecção de variáveis fora de escopo.
- `testes.c`: string sendo atribuida em variável int gera erro.
- `missing_return.c`: função sem `return` obrigatório reporta erro.

# Membros

- Andre Lopes de Sousa - 211031593
- João Victor da Silva Batista de Farias - 221022604
- Livia Rodrigues Reis - 180105051
- Marco Marques de Castro - 211062197
- Sophia Souza da Silva - 231026886