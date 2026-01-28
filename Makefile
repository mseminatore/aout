# Makefile for aout C library

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I. -Ihash -Itesty
LDFLAGS = 

# Source files
HASH_SRC = hash/hash.c
TEST_MAIN_SRC = testy/test_main.c
AOUT_SRC = aout.c
TEST_SRC = test_aout.c

# Object files
HASH_OBJ = hash/hash.o
TEST_MAIN_OBJ = testy/test_main.o
AOUT_OBJ = aout.o
TEST_OBJ = test_aout.o

# Targets
TEST_BIN = test_aout
LIB = libaout.a

.PHONY: all clean test

all: $(LIB) $(TEST_BIN)

# Build static library
$(LIB): $(AOUT_OBJ) $(HASH_OBJ)
	ar rcs $@ $^

# Build test executable
$(TEST_BIN): $(TEST_OBJ) $(AOUT_OBJ) $(HASH_OBJ) $(TEST_MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run tests
test: $(TEST_BIN)
	./$(TEST_BIN)

# Clean build artifacts
clean:
	rm -f $(AOUT_OBJ) $(TEST_OBJ) $(HASH_OBJ) $(TEST_MAIN_OBJ)
	rm -f $(LIB) $(TEST_BIN)
	rm -f test_output.o

# Dependencies
$(AOUT_OBJ): aout.c aout.h hash/hash.h
$(HASH_OBJ): hash/hash.c hash/hash.h
$(TEST_OBJ): test_aout.c aout.h testy/test.h
$(TEST_MAIN_OBJ): testy/test_main.c testy/test.h
