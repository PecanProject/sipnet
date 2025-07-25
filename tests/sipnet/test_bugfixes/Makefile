# gotta be a better way to do this for ROOT_DIR
ROOT_DIR=../../..

CC=gcc
LD=gcc
CFLAGS=-Wall -g -I$(ROOT_DIR)/src -I$(ROOT_DIR)/tests -Wno-c2x-extensions
LDFLAGS=-L$(ROOT_DIR)/libs
LDLIBS=-lsipnet -lsipnet_common -lm

# List test files in this directory here
TEST_CFILES=testEventFileOrderChecks.c

# The rest is boilerplate, likely copyable as is to a new test directory
TEST_OBJ_FILES=$(TEST_CFILES:%.c=%.o)
TEST_EXECUTABLES:=$(basename $(TEST_CFILES))
RUN_EXECUTABLES:= $(addsuffix .run, $(TEST_EXECUTABLES))

TEST_DEPS:=tUtils.h exitHandler.c
TEST_DEPS:=$(addprefix $(ROOT_DIR)/tests/utils/, $(TEST_DEPS))

LIB_DEPS:=libsipnet.a libsipnet_common.a
LIB_DEPS:=$(addprefix $(ROOT_DIR)/libs/, $(LIB_DEPS))

all: tests

tests: $(TEST_EXECUTABLES)

$(TEST_EXECUTABLES): %: %.o
	$(CC) $(LDFLAGS) $< $(LDLIBS) -o $@

# This will error (intentionally) if the library has not been built; when running
# this manually, make sure to build it from the top-level makefile first
$(TEST_OBJ_FILES): $(TEST_DEPS) $(LIB_DEPS)
$(TEST_OBJ_FILES): %.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(RUN_EXECUTABLES)

$(RUN_EXECUTABLES):
	./$(basename $@)

clean:
	rm -f $(TEST_OBJ_FILES) $(TEST_EXECUTABLES) events.in events.out

.PHONY: all tests clean run $(RUN_EXECUTABLES)
