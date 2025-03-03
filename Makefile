CC=gcc
LD=gcc
AR=libtool
CFLAGS=-Wall -Werror -g -I./src
LIBLINKS=-lm
LIB_DIR=./libs
LDFLAGS=-L$(LIB_DIR)

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

SENSTEST_CFILES:=sensTest.c
SENSTEST_CFILES:=$(addprefix src/sensTest/, $(SENSTEST_CFILES))
SENSTEST_OFILES=$(SENSTEST_CFILES:.c=.o)
SENSTEST_LIBS=-lsipnet_common -lsipnet

TRANSPOSE_CFILES:=transpose.c
TRANSPOSE_CFILES:=$(addprefix src/transpose/, $(TRANSPOSE_CFILES))
TRANSPOSE_OFILES=$(TRANSPOSE_CFILES:.c=.o)
TRANSPOSE_LIBS=-lsipnet_common

SUBSET_DATA_CFILES:=subsetData.c
SUBSET_DATA_CFILES:=$(addprefix src/subsetData/, $(SUBSET_DATA_CFILES))
SUBSET_DATA_OFILES=$(SUBSET_DATA_CFILES:.c=.o)
SUBSET_DATA_LIBS=-lsipnet_common

# one-off build for the hist file utils
HISTUTIL_CFILES:=bintotxt.c txttobin.c
HISTUTIL_CFILES:=$(addprefix src/common/, $(HISTUTIL_CFILES))
HISTUTIL_OFILES=$(HISTUTIL_CFILES:.c=.o)
HISTUTIL_LIBS=-lsipnet_common

CFILES=$(sort  \
    $(ESTIMATE_CFILES) $(SENSTEST_CFILES) $(SIPNET_CFILES) $(TRANSPOSE_CFILES) $(SUBSET_DATA_CFILES) \
    $(HISTUTIL_CFILES) \
    )

SIPNET_LIB=libsipnet.a
COMMON_LIB=libsipnet_common.a

# Doxygen
DOXYFILE = docs/Doxyfile
DOXYGEN_HTML_DIR = docs/html
DOXYGEN_LATEX_DIR = docs/latex

# the make command, with no arguments, should not build everything in this complex
# environment
.DEFAULT_GOAL := sipnet

# with the default set to 'sipnet', we can have all do (close to) everything
all: exec document

# the main executables - the original 'all' target
exec: estimate sipnet transpose subsetData # sensTest

# Only update docs if source files or Doxyfile have changed
document: .doxygen.stamp

.doxygen.stamp: $(CFILES) $(DOXYFILE)
	@echo "Running Doxygen..."
	doxygen $(DOXYFILE)
	@touch .doxygen.stamp

$(COMMON_LIB): $(COMMON_OFILES)
	$(AR) -static -o $(LIB_DIR)/$(COMMON_LIB) $(COMMON_OFILES)

$(SIPNET_LIB): $(SIPNET_OFILES)
	$(AR) -static -o $(LIB_DIR)/$(SIPNET_LIB) $(SIPNET_OFILES)

sipnet: $(SIPNET_OFILES) $(COMMON_LIB)
	$(LD) $(LDFLAGS) -o sipnet $(SIPNET_OFILES) $(LIBLINKS) $(SIPNET_LIBS)

estimate: $(ESTIMATE_OFILES) $(COMMON_LIB) $(SIPNET_LIB)
	$(LD) $(LDFLAGS) -o estimate $(ESTIMATE_OFILES) $(LIBLINKS) $(ESTIMATE_LIBS)

#sensTest: $(SENSTEST_OFILES) $(SENSTEST_LIBS) $(COMMON_LIB) $(SIPNET_LIB)
#	$(LD) $(LDFLAGS) -o sensTest $(SENSTEST_OFILES) $(LIBLINKS) $(SENSTEST_LIBS)

transpose: $(TRANSPOSE_OFILES) $(COMMON_LIB)
	$(LD) $(LDFLAGS) -o transpose $(TRANSPOSE_OFILES) $(LIBLINKS) $(TRANSPOSE_LIBS)

subsetData: $(SUBSET_DATA_OFILES) $(COMMON_LIB)
	$(LD) $(LDFLAGS) -o subsetData $(SUBSET_DATA_OFILES) $(LIBLINKS) $(SUBSET_DATA_LIBS)

histutil: $(HISTUTIL_OFILES) $(COMMON_LIB)
	$(LD) $(LDFLAGS) -o bintotxt src/common/bintotxt.o $(HISTUTIL_LIBS)
	$(LD) $(LDFLAGS) -o txttobin src/common/txttobin.o $(HISTUTIL_LIBS)

clean:
	rm -f $(ESTIMATE_OFILES) $(SIPNET_OFILES) $(TRANSPOSE_OFILES) $(SUBSET_DATA_OFILES) $(COMMON_OFILES)
	rm -f $(HISTUTIL_OFILES)
	rm -f estimate sensTest sipnet transpose subsetData $(COMMON_LIB) $(SIPNET_LIB) bintotxt txttobin
	rm -rf $(DOXYGEN_HTML_DIR) $(DOXYGEN_LATEX_DIR)

# UNIT TESTS
SIPNET_TEST_DIRS:=$(shell find tests/sipnet -type d -mindepth 1 -maxdepth 1)
SIPNET_TEST_DIRS_RUN:= $(addsuffix .run, $(SIPNET_TEST_DIRS))
SIPNET_TEST_DIRS_CLEAN:= $(addsuffix .clean, $(SIPNET_TEST_DIRS))

test: $(SIPNET_TEST_DIRS) $(SIPNET_LIB)

# The dash in the build command tells make to continue if there are errors, allowing cleanup
$(SIPNET_TEST_DIRS): $(SIPNET_LIB)
	-$(MAKE) -C $@

testrun: $(SIPNET_TEST_DIRS_RUN)

$(SIPNET_TEST_DIRS_RUN):
	$(MAKE) -C $(basename $@) run

testclean: $(SIPNET_TEST_DIRS_CLEAN)
	rm -f $(SIPNET_LIB)

$(SIPNET_TEST_DIRS_CLEAN):
	$(MAKE) -C $(basename $@) clean

.PHONY: all clean document estimate sipnet transpose subsetData doxygen \
		test $(SIPNET_TEST_DIRS) $(SIPNET_LIB) testrun \
		$(SIPNET_TEST_DIRS_RUN) testclean $(SIPNET_TEST_DIRS_CLEAN) help

help:
	@echo "Available make targets:"
	@echo "  help         - Display this help message."
	@echo "  === Core Targets ==="
	@echo "  (default)    - Build the sipnet executable"
	@echo "  <executable> - Build an individual executable; available: sipnet, estimate, sensTest, transpose"
	@echo "                 See below for more information"
	@echo "  exec         - Build all of the above executables"
	@echo "  document     - Generate documentation (via doxygen)"
	@echo "  all          - Build all above executables and the documentation"
	@echo "  clean        - Remove compiled files, executables, and documentation"
	@echo "  depend       - Generate build dependency information for source files and append to Makefile"
	@echo "  === Tool Info ==="
	@echo "  sipnet       - main model program"
	@echo "  estimate     - estimate parameters using MCMC"
	@echo "  transpose    - read in and transpose a matrix"
	@echo "  subsetData   - subset input files (e.g., .clim, .dat, .valid, .sigma, .spd) from a specified start date"
	@echo "                 and length of time in days"
	@echo " === Tests ==="
	@echo "  test         - Build the unit tests"
	@echo "  testrun      - Run the unit tests"
	@echo "  testclean    - Clean build artifacts and executables from the unit tests"
	@echo "  === Additional Utils ==="
	@echo "  histutils    - Build bintotxt and txttobin, utilities to convert the hist file output from an estimate run"
	@echo "                 from a binary file to a text file, and back again"

depend::
	makedepend $(CFILES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

