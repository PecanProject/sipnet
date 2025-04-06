CC=gcc
LD=gcc
AR=ar -rs
CFLAGS=-Wall -Werror -g -Isrc
LIBLINKS=-lm
LIB_DIR=./libs
LDFLAGS=-L$(LIB_DIR)

# Main executables
COMMON_CFILES:=util.c paramchange.c namelistInput.c spatialParams.c
COMMON_CFILES:=$(addprefix src/common/, $(COMMON_CFILES))
COMMON_OFILES=$(COMMON_CFILES:.c=.o)

SIPNET_CFILES:=sipnet.c frontend.c runmean.c outputItems.c events.c
SIPNET_CFILES:=$(addprefix src/sipnet/, $(SIPNET_CFILES))
SIPNET_OFILES=$(SIPNET_CFILES:.c=.o)
SIPNET_LIBS=-lsipnet_common

ESTIMATE_CFILES:=ml-metro5.c ml-metrorun.c
ESTIMATE_CFILES:=$(addprefix src/estimate/, $(ESTIMATE_CFILES))
ESTIMATE_OFILES=$(ESTIMATE_CFILES:.c=.o)
ESTIMATE_LIBS=-lsipnet_common -lsipnet

# Utilities
TRANSPOSE_CFILES:=transpose.c
TRANSPOSE_CFILES:=$(addprefix src/utilities/, $(TRANSPOSE_CFILES))
TRANSPOSE_OFILES=$(TRANSPOSE_CFILES:.c=.o)
TRANSPOSE_LIBS=-lsipnet_common

SUBSET_DATA_CFILES:=subsetData.c
SUBSET_DATA_CFILES:=$(addprefix src/utilities/, $(SUBSET_DATA_CFILES))
SUBSET_DATA_OFILES=$(SUBSET_DATA_CFILES:.c=.o)
SUBSET_DATA_LIBS=-lsipnet_common

HISTUTIL_CFILES:=bintotxt.c txttobin.c
HISTUTIL_CFILES:=$(addprefix src/utilities/, $(HISTUTIL_CFILES))
HISTUTIL_OFILES=$(HISTUTIL_CFILES:.c=.o)
HISTUTIL_LIBS=-lsipnet_common

CFILES=$(sort  \
    $(ESTIMATE_CFILES) $(SENSTEST_CFILES) $(SIPNET_CFILES) $(TRANSPOSE_CFILES) $(SUBSET_DATA_CFILES) \
    $(HISTUTIL_CFILES) \
    )

SIPNET_LIB=$(LIB_DIR)/libsipnet.a
COMMON_LIB=$(LIB_DIR)/libsipnet_common.a

# Doxygen
DOXYFILE = docs/Doxyfile
DOXYGEN_HTML_DIR = docs/html
DOXYGEN_LATEX_DIR = docs/latex

# the make command, with no arguments, should not build everything in this complex
# environment
.DEFAULT_GOAL := sipnet

# the main executables - the original 'all' target
exec: estimate sipnet transpose subsetData histutil

# with the default set to 'sipnet', we can have all do (close to) everything
all: exec document

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

sipnet: $(SIPNET_OFILES) $(COMMON_LIB)
	$(LD) $(LDFLAGS) -o sipnet $(SIPNET_OFILES) $(LIBLINKS) $(SIPNET_LIBS)

estimate: $(ESTIMATE_OFILES) $(COMMON_LIB) $(SIPNET_LIB)
	$(LD) $(LDFLAGS) -o estimate $(ESTIMATE_OFILES) $(LIBLINKS) $(ESTIMATE_LIBS)

transpose: $(TRANSPOSE_OFILES) $(COMMON_LIB)
	$(LD) $(LDFLAGS) -o transpose $(TRANSPOSE_OFILES) $(LIBLINKS) $(TRANSPOSE_LIBS)

subsetData: $(SUBSET_DATA_OFILES) $(COMMON_LIB)
	$(LD) $(LDFLAGS) -o subsetData $(SUBSET_DATA_OFILES) $(LIBLINKS) $(SUBSET_DATA_LIBS)

histutil: $(HISTUTIL_OFILES) $(COMMON_LIB)
	$(LD) $(LDFLAGS) -o bintotxt src/common/bintotxt.o $(HISTUTIL_LIBS)
	$(LD) $(LDFLAGS) -o txttobin src/common/txttobin.o $(HISTUTIL_LIBS)

clean:
	rm -f $(ESTIMATE_OFILES) $(SIPNET_OFILES) $(TRANSPOSE_OFILES) $(SUBSET_DATA_OFILES) $(COMMON_OFILES)
	rm -f $(HISTUTIL_OFILES) $(COMMON_LIB) $(SIPNET_LIB)
	rm -f estimate sensTest sipnet transpose subsetData bintotxt txttobin
	rm -rf $(DOXYGEN_HTML_DIR) $(DOXYGEN_LATEX_DIR)
	rm -rf site/

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

.PHONY: all clean histutil help document exec cleanall \
		test $(SIPNET_TEST_DIRS) $(SIPNET_TEST_DIRS_RUN) testclean $(SIPNET_TEST_DIRS_CLEAN) testrun

help:
	@echo "Available make targets:"
	@echo "  help         - Display this help message."
	@echo "  === Core Targets ==="
	@echo "  sipnet       - (also default target) Build the sipnet executable; see sipnet.in in the src/sipnet"
	@echo "                 directory for a sample input file"
	@echo "  estimate     - Build the estimate executable (estimates parameters using MCMC); see estimate.in in the "
	@echo "                 src/estimate directory for sample input file"
	@echo "  exec         - Build the above executables and all utilities (see below)"
	@echo "  document     - Generate documentation (via doxygen and mkdocs)"
	@echo "  all          - Build all above executables and the documentation"
	@echo "  clean        - Remove compiled files, executables, and documentation"
	@echo "  depend       - Generate build dependency information for source files and append to Makefile"
	@echo "  === Utilities ==="
	@echo "  transpose    - read in and transpose a matrix"
	@echo "  subsetData   - subset input files (e.g., .clim, .dat, .valid, .sigma, .spd) from a specified start date"
	@echo "                 and length of time in days (see src/utilities/subset.in for sample input file)"
	@echo "  histutil     - Build bintotxt and txttobin, utilities to convert the hist file output from an estimate"
	@echo "                 run from a binary file to a text file, and back again"
	@echo "  === Tests ==="
	@echo "  test         - Build the unit tests"
	@echo "  testrun      - Run the unit tests"
	@echo "  testclean    - Clean build artifacts and executables from the unit tests"

depend::
	makedepend $(CFILES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
