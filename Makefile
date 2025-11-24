CC = gcc
LEX = flex
YACC = bison
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc -I.
TARGET = c2lua

SRC = src/main.c \
	  src/ast.c \
	  src/symbol_table.c \
	src/semantic.c \
	src/codegen_lua.c \
	src/optimizer.c
LEX_SRC = src/lexer.l
YACC_SRC = src/parser.y

LEX_OUT = lex.yy.c
YACC_OUT = parser.tab.c
YACC_HEADER = parser.tab.h

PASS_DIR = tests/pass
PASS_SOURCES := $(wildcard $(PASS_DIR)/*.c)
PASS_CASES := $(basename $(notdir $(PASS_SOURCES)))
FAIL_DIR = tests/fail
FAIL_SOURCES := $(wildcard $(FAIL_DIR)/*.c)
FAIL_CASES := $(basename $(notdir $(FAIL_SOURCES)))

all: $(TARGET)

$(TARGET): $(LEX_OUT) $(YACC_OUT) $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(LEX_OUT) $(YACC_OUT) $(SRC) -lfl

$(LEX_OUT): $(LEX_SRC) $(YACC_HEADER)
	$(LEX) $(LEX_SRC)

$(YACC_OUT) $(YACC_HEADER): $(YACC_SRC) src/ast.h src/ast.c
	cd src && $(YACC) -d --defines=../parser.tab.h -o ../parser.tab.c parser.y

clean:
	rm -f $(TARGET) $(LEX_OUT) $(YACC_OUT) $(YACC_HEADER)

test: test-pass test-fail

test-pass: all
	@echo "== Running pass tests =="
	@if [ -z "$(PASS_CASES)" ]; then \
		echo "No pass tests found under $(PASS_DIR)"; \
	else \
		for case in $(PASS_CASES); do \
			input="$(PASS_DIR)/$$case.c"; \
			expected_file="$(PASS_DIR)/$$case.lua"; \
			if [ ! -f "$$expected_file" ]; then \
				echo "Missing expected Lua file for $$case"; \
				exit 1; \
			fi; \
			output=$$(./c2lua "$$input"); \
			expected=$$(cat "$$expected_file"); \
			printf '%s' "-- $$case... "; \
			if [ "$$output" = "$$expected" ]; then \
				echo "ok"; \
			else \
				echo "fail"; \
				printf 'Expected:\n%s\n\nGot:\n%s\n' "$$expected" "$$output"; \
				exit 1; \
			fi; \
		done; \
		echo "All pass tests passed."; \
	fi

test-fail: all
	@echo "== Running fail tests =="
	@if [ -z "$(FAIL_CASES)" ]; then \
		echo "No fail tests found under $(FAIL_DIR)"; \
	else \
		for case in $(FAIL_CASES); do \
			input="$(FAIL_DIR)/$$case.c"; \
			expected_err="$(FAIL_DIR)/$$case.err"; \
			if [ ! -f "$$expected_err" ]; then \
				echo "Missing expected .err file for $$case"; \
				exit 1; \
			fi; \
			tmp_err=$$(mktemp); \
			if ./c2lua "$$input" > /dev/null 2> "$$tmp_err"; then \
				echo "-- $$case... fail"; \
				echo "Expected compilation error but succeeded."; \
				rm -f "$$tmp_err"; \
				exit 1; \
			fi; \
			if diff -u "$$expected_err" "$$tmp_err" > /dev/null; then \
				echo "-- $$case... ok"; \
			else \
				echo "-- $$case... fail"; \
				echo "Expected:"; \
				cat "$$expected_err"; \
				echo; \
				echo "Got:"; \
				cat "$$tmp_err"; \
				rm -f "$$tmp_err"; \
				exit 1; \
			fi; \
			rm -f "$$tmp_err"; \
		done; \
		echo "All fail tests passed."; \
	fi
