CC=gcc
LD=gcc
CFLAGS=-Wall
LIBLINKS=-lm

ESTIMATE_CFILES=sipnet.c ml-metro5.c ml-metrorun.c paramchange.c runmean.c util.c spatialParams.c namelistInput.c outputItems.c
ESTIMATE_OFILES=$(ESTIMATE_CFILES:.c=.o)

SENSTEST_CFILES=sipnet.c sensTest.c paramchange.c runmean.c util.c spatialParams.c namelistInput.c outputItems.c
SENSTEST_OFILES=$(SENSTEST_CFILES:.c=.o)

SIPNET_CFILES=sipnet.c frontend.c runmean.c util.c spatialParams.c namelistInput.c outputItems.c
SIPNET_OFILES=$(SIPNET_CFILES:.c=.o)

TRANSPOSE_CFILES=transpose.c util.c
TRANSPOSE_OFILES=$(TRANSPOSE_CFILES:.c=.o)

SUBSET_DATA_CFILES=subsetData.c util.c namelistInput.c
SUBSET_DATA_OFILES=$(SUBSET_DATA_CFILES:.c=.o)

# all: estimate sensTest sipnet transpose subsetData
all: estimate sipnet transpose subsetData

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

#clean:
#	rm -f $(ESTIMATE_OFILES) $(SENSTEST_OFILES) $(SIPNET_OFILES) $(TRANSPOSE_OFILES) $(SUBSET_DATA_OFILES) estimate sensTest  sipnet transpose subsetData


#This target automatically builds dependencies.
depend::
	makedepend $(CFILES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

