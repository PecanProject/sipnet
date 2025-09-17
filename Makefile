CC=gcc
LD=gcc
AR=ar -rs
CFLAGS=-Wall -Werror -g -Isrc -Wno-c2x-extensions -DGIT_HASH='$(GIT_HASH)'
LIBLINKS=-lm
LIB_DIR=./libs
LDFLAGS=-L$(LIB_DIR)

# Main executables
COMMON_CFILES:=context.c logging.c modelParams.c util.c
COMMON_CFILES:=$(addprefix src/common/, $(COMMON_CFILES))
COMMON_OFILES=$(COMMON_CFILES:.c=.o)

SIPNET_CFILES:=sipnet.c cli.c events.c frontend.c outputItems.c runmean.c state.c
SIPNET_CFILES:=$(addprefix src/sipnet/, $(SIPNET_CFILES))
SIPNET_OFILES=$(SIPNET_CFILES:.c=.o)
SIPNET_LIBS=-lsipnet_common

CFILES=$(sort $(SIPNET_CFILES))

SIPNET_LIB=$(LIB_DIR)/libsipnet.a
COMMON_LIB=$(LIB_DIR)/libsipnet_common.a

# Doxygen
DOXYFILE = docs/Doxyfile
DOXYGEN_HTML_DIR = docs/api
DOXYGEN_LATEX_DIR = docs/latex

# the make command, with no arguments, should not build everything in this complex
# environment
.DEFAULT_GOAL := sipnet

# Look up what Git revision we're building from
# We use this below to compile the hash into the binary for ease of debugging
GIT_HASH := $(shell git rev-parse --short=10 HEAD; git diff-index --quiet HEAD || echo " plus uncommitted changes")

# all does everything build related (but not test)
all: sipnet document

# Build documentation with both Doxygen and Mkdocs
document: .doxygen.stamp .mkdocs.stamp

# Only update docs if source files or Doxyfile have changed
.doxygen.stamp: $(DOXYFILE) $(CFILES)
	@if [ ! -d $(DOXYGEN_HTML_DIR) ] || [ $(DOXYFILE) -nt .doxygen.stamp ] || \
	   find $(CFILES) -newer .doxygen.stamp | read dummy; then \
		echo "Running Doxygen..."; \
		doxygen $(DOXYFILE); \
		touch .doxygen.stamp; \
	else \
		echo "Doxygen is up-to-date."; \
	fi

.mkdocs.stamp: .doxygen.stamp
	@echo "Running Mkdocs to build site..."
	mkdocs build
	@touch .mkdocs.stamp

$(COMMON_LIB): $(COMMON_OFILES)
	$(AR) $(COMMON_LIB) $(COMMON_OFILES)

$(SIPNET_LIB): $(SIPNET_OFILES)
	$(AR) $(SIPNET_LIB) $(SIPNET_OFILES)

GCC_VERSION = $(shell $(CC) --version)
info:
	@echo "System info"
	@echo "ARCH: $(shell arch)"
	@echo "CC: $(GCC_VERSION)"
	@echo ""

sipnet: info $(SIPNET_OFILES) $(COMMON_LIB)
	$(LD) $(LDFLAGS) -o sipnet $(SIPNET_OFILES) $(LIBLINKS) $(SIPNET_LIBS)

clean:
	rm -f $(SIPNET_OFILES) $(COMMON_OFILES)
	rm -f $(COMMON_LIB) $(SIPNET_LIB)
	rm -f sipnet
	rm -rf $(DOXYGEN_HTML_DIR) $(DOXYGEN_LATEX_DIR)
	rm -rf site/
	rm -f .doxygen.stamp .mkdocs.stamp

# UNIT TESTS
SIPNET_TEST_DIRS:=$(shell find tests/sipnet -type d -mindepth 1 -maxdepth 1)
SIPNET_TEST_DIRS_RUN:= $(addsuffix .run, $(SIPNET_TEST_DIRS))
SIPNET_TEST_DIRS_CLEAN:= $(addsuffix .clean, $(SIPNET_TEST_DIRS))

test: $(SIPNET_TEST_DIRS) $(COMMON_LIB) $(SIPNET_LIB)

# The dash in the build command tells make to continue if there are errors, allowing cleanup
$(SIPNET_TEST_DIRS): $(SIPNET_LIB) $(COMMON_LIB)
	-$(MAKE) -C $@

testrun: $(SIPNET_TEST_DIRS_RUN)

$(SIPNET_TEST_DIRS_RUN):
	$(MAKE) -C $(basename $@) run

testclean: $(SIPNET_TEST_DIRS_CLEAN)
#	rm -f $(SIPNET_LIB)

$(SIPNET_TEST_DIRS_CLEAN):
	$(MAKE) -C $(basename $@) clean

cleanall: clean testclean

.PHONY: all clean help document exec cleanall \
		test $(SIPNET_TEST_DIRS) $(SIPNET_TEST_DIRS_RUN) testclean $(SIPNET_TEST_DIRS_CLEAN) testrun

help:
	@echo "Available make targets:"
	@echo "  help         - Display this help message."
	@echo "  === Core Targets ==="
	@echo "  sipnet       - (also default target) Build the sipnet executable; see sipnet.in in the src/sipnet"
	@echo "                 directory for a sample input file"
	@echo "  document     - Generate documentation (via doxygen and mkdocs)"
	@echo "  all          - Build sipnet executable and the documentation"
	@echo "  clean        - Remove compiled files, executables, and documentation"
	@echo "  depend       - Generate build dependency information for source files and append to Makefile"
	@echo "  === Tests ==="
	@echo "  test         - Build the unit tests"
	@echo "  testrun      - Run the unit tests"
	@echo "  testclean    - Clean build artifacts and executables from the unit tests"
	@echo "  cleanall     - Run both clean and testclean"


# Make sure this target and subsequent comment remain at the bottom of this file
depend::
	makedepend $(CFILES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
