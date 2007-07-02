Typing 'make' in this directory will make the following executables:

MAIN PROGRAMS:

sipnet: The program used for forward runs of the model. Its operation is
controlled by the file 'sipnet.in'. SIPNET can run in three different
modes: 'standard', for a standard forward run; 'senstest', for
performing a sensitivity test on a single parameter; and 'montecarlo',
for running the model forward using an ensemble of parameter sets. See
'sipnet.in' for further configuration options for each of these types of
runs.

estimate: The parameter estimation program for SIPNET. Its operation is
controlled by the file 'estimate.in'.

sensTest: A program that tests the sensitivity of the model-data fit to
values of a parameter (essentially a one-dimensional parameter
estimation). Its operation is controlled by the file 'sensTest.in'. 

VARIANTS OF ABOVE:
(WJS 7-1-2007: These have not been fully tested for compatability with
all of the changes of the last few months. In particular, options that
require you to specify parameter indices (e.g. sensTest) may not behave
as expected.)

sipnetGirdle: A modified version of SIPNET. Its operation is controlled
by command-line arguments. WJS (7-1-2007): I have not changed this
program to stay up to date with the recent changes made to SIPNET, and
have not tested it after making these changes. Use at your own risk!

sensTestFull: sensTest as modified by John Zobitz. Its operation is
controlled by command-line arguments. WJS (7-1-2007): I have not tested
this program with the changes I have made over the last few months (in
particular, the new version of spatialParams), and thus do not guarantee
that it will work as expected. Use at your own risk!

STAND-ALONE UTILITIES

subsetData: A utility to subset a data record temporally. This utility
can also perform daily aggregation on the data if so desired. Its
operation is controlled by subset.in.

transpose: A little utility to transpose a text file that contains a
rectangular matrix of data. This may be necessary to read certain files
into Excel (e.g. the single-variable output files). Its usage is
'transpose filename'.



OTHER UTILITIES (NOT BUILT WITH MAKEFILE)

bintotxt.c, txttobin.c: Utilities to convert the hist file output from a
estimate run from a binary file to a text file, and back again.
