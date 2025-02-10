CC=gcc
LD=gcc
AR=ar
CFLAGS=-Wall
LIBLINKS=-lm

ESTIMATE_CFILES=sipnet.c ml-metro5.c ml-metrorun.c paramchange.c runmean.c util.c spatialParams.c namelistInput.c outputItems.c events.c
ESTIMATE_OFILES=$(ESTIMATE_CFILES:.c=.o)

SENSTEST_CFILES=sipnet.c sensTest.c paramchange.c runmean.c util.c spatialParams.c namelistInput.c outputItems.c events.c
SENSTEST_OFILES=$(SENSTEST_CFILES:.c=.o)

SIPNET_CFILES=sipnet.c frontend.c runmean.c util.c spatialParams.c namelistInput.c outputItems.c events.c
SIPNET_OFILES=$(SIPNET_CFILES:.c=.o)

TRANSPOSE_CFILES=transpose.c util.c
TRANSPOSE_OFILES=$(TRANSPOSE_CFILES:.c=.o)

SUBSET_DATA_CFILES=subsetData.c util.c namelistInput.c
SUBSET_DATA_OFILES=$(SUBSET_DATA_CFILES:.c=.o)

CFILES=$(sort $(ESTIMATE_CFILES) $(SENSTEST_CFILES) $(SIPNET_CFILES) $(TRANSPOSE_CFILES) $(SUBSET_DATA_CFILES))

# Doxygen
DOXYFILE = docs/Doxyfile
DOXYGEN_HTML_DIR = docs/html
DOXYGEN_LATEX_DIR = docs/latex

all: estimate sipnet transpose subsetData document

# Only update docs if source files or Doxyfile have changed
document: .doxygen.stamp

.doxygen.stamp: $(CFILES) $(DOXYFILE)
	@echo "Running Doxygen..."
	doxygen $(DOXYFILE)
	@touch .doxygen.stamp

estimate: $(ESTIMATE_OFILES)
	$(LD) -o estimate $(ESTIMATE_OFILES) $(LIBLINKS)

#sensTest: $(SENSTEST_OFILES)
#	$(LD) -o sensTest $(SENSTEST_OFILES) $(LIBLINKS)

sipnet: $(SIPNET_OFILES)
	$(LD) -o sipnet $(SIPNET_OFILES) $(LIBLINKS)

transpose: $(TRANSPOSE_OFILES)
	$(LD) -o transpose $(TRANSPOSE_OFILES) $(LIBLINKS)

subsetData: $(SUBSET_DATA_OFILES)
	$(LD) -o subsetData $(SUBSET_DATA_OFILES) $(LIBLINKS)

clean:
	rm -f $(ESTIMATE_OFILES) $(SIPNET_OFILES) $(TRANSPOSE_OFILES) $(SUBSET_DATA_OFILES) estimate sensTest  sipnet transpose subsetData
	rm -rf $(DOXYGEN_HTML_DIR) $(DOXYGEN_LATEX_DIR)

# UNIT TESTS
SIPNET_TEST_DIRS:=$(shell find tests/sipnet -type d -mindepth 1 -maxdepth 1)
SIPNET_TEST_DIRS_RUN:= $(addsuffix .run, $(SIPNET_TEST_DIRS))
SIPNET_TEST_DIRS_CLEAN:= $(addsuffix .clean, $(SIPNET_TEST_DIRS))
SIPNET_LIB=libsipnet.a

$(SIPNET_LIB): $(SIPNET_LIB)($(SIPNET_OFILES))
	ranlib $(SIPNET_LIB)

test: pretest $(SIPNET_TEST_DIRS) posttest $(SIPNET_LIB)

pretest:
	cp modelStructures.h modelStructures.orig.h

# The dash in the build command tells make to continue if there are errors, allowing cleanup
$(SIPNET_TEST_DIRS): pretest $(SIPNET_LIB)
	cp $@/modelStructures.h modelStructures.h
	-$(MAKE) -C $@

# This is far from infallible, as model_structures.h will be in a bad place if a test
# build step fails in a non-catchable way
posttest: $(SIPNET_TEST_DIRS)
	mv modelStructures.orig.h modelStructures.h

testrun: $(SIPNET_TEST_DIRS_RUN)

$(SIPNET_TEST_DIRS_RUN):
	$(MAKE) -C $(basename $@) run

testclean: $(SIPNET_TEST_DIRS_CLEAN)
	rm -f $(SIPNET_LIB)

$(SIPNET_TEST_DIRS_CLEAN):
	$(MAKE) -C $(basename $@) clean

.PHONY: all clean document estimate sipnet transpose subsetData doxygen \
		test $(SIPNET_TEST_DIRS) pretest posttest $(SIPNET_LIB) testrun \
		$(SIPNET_TEST_DIRS_RUN) testclean $(SIPNET_TEST_DIRS_CLEAN) help

help:
	@echo "Available targets:"
	@echo "  help        - Display this help message."
	@echo "  === core targets ==="
	@echo "  all         - Builds all components."
	@echo "  document    - Generate documentation."
	@echo "  sipnet      - Builds the 'sipnet' executable."
	@echo "  clean       - Removes compiled files, executables, and documentation."
	@echo "  depend      - Automatically generates dependency information for source files."
	@echo "  === additional tools ==="
	@echo "  estimate    - Builds 'estimate' executable to estimate parameters using MCMC."
	@echo "  transpose   - Builds 'transpose' executable to read in and transpose a matrix"
	@echo "  subsetData  - Builds 'subsetData' executable that subsets input files (e.g., .clim, .dat, .valid, .sigma, .spd) from a specified start date and length of time in days."

depend::
	makedepend $(CFILES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

