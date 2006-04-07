CC=gcc
LD=gcc
CFLAGS=-Wall
LIBLINKS=-lm

ESTIMATE_CFILES=sipnet.c ml-metro5.c ml-metrorun.c paramchange.c runmean.c util.c spatialParams.c
ESTIMATE_OFILES=$(ESTIMATE_CFILES:.c=.o)
SENSTEST_CFILES=sipnet.c sensTest.c paramchange.c runmean.c util.c spatialParams.c
SENSTEST_OFILES=$(SENSTEST_CFILES:.c=.o)
SIPNET_CFILES=sipnet.c frontend.c runmean.c util.c spatialParams.c
SIPNET_OFILES=$(SIPNET_CFILES:.c=.o)

all: estimate sensTest sipnet

estimate: $(ESTIMATE_OFILES)
	$(LD) $(LIBLINKS) -o estimate $(ESTIMATE_OFILES)

sensTest: $(SENSTEST_OFILES)
	$(LD) $(LIBLINKS) -o sensTest $(SENSTEST_OFILES)

sipnet: $(SIPNET_OFILES)
	$(LD) $(LIBLINKS) -o sipnet $(SIPNET_OFILES)

clean:
	rm -f $(ESTIMATE_OFILES) $(SENSTEST_OFILES) $(SIPNET_OFILES) estimate sensTest sipnet

#This target automatically builds dependencies.
depend::
	makedepend $(CFILES)

# DO NOT DELETE THIS LINE -- make depend depends on it.

