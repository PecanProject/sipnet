CC=gcc
LD=gcc
CFLAGS=-Wall -g
LDFLAGS=-L../../..
LDLIBS=-lsipnet

# List test files in this directory here
TEST_CFILES=testEventInfra.c
TEST_OBJ_FILES=$(TEST_CFILES:%.c=%.o)
TEST_EXECUTABLES:=$(basename $(TEST_CFILES))
RUN_EXECUTABLES:= $(addsuffix .run, $(TEST_EXECUTABLES))

all: tests

tests: $(TEST_EXECUTABLES)

$(TEST_EXECUTABLES): %: %.o
	$(CC) $(LDFLAGS) $< $(LDLIBS) -o $@

$(TEST_OBJ_FILES): ../utils.h ../../../libsipnet.a
$(TEST_OBJ_FILES): %.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(RUN_EXECUTABLES)

$(RUN_EXECUTABLES):
	./$(basename $@)

clean:
	rm -f $(TEST_OBJ_FILES) $(TEST_EXECUTABLES) events.in

.PHONY: all tests clean run $(RUN_EXECUTABLES)
