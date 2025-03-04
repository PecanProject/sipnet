# SIPNET

(short blurb)

## Introduction

(Insert SIPNET blurb here; maybe with a reference or two?)

## Getting Started

### Installing

What's required? 

[Python may be required in the future, depending on what happens with formatting pre-commit hooks; we'll see]

### Executing the Program

blah blah blah

## Contributing

See the main [Contributing](/CONTRIBUTING.md) page

-----
### Old stuff...

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

STAND-ALONE UTILITIES

subsetData: A utility to subset a data record temporally. This utility
can also perform daily aggregation on the data if so desired. Its
operation is controlled by subset.in.

transpose: A little utility to transpose a text file that contains a
rectangular matrix of data. This may be necessary to read certain files
into Excel (e.g. the single-variable output files). Its usage is
'transpose filename'.

bintotxt, txttobin: Utilities to convert the hist file output from a
estimate run from a binary file to a text file, and back again.
