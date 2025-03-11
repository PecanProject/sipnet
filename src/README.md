# Organization and Packages

Organization strategy:
* sipnet and derivative programs each have their own directory
* stand-alone utilities all go into the `utilities` subdirectory

## Main program - SIPNET

[Add description here of SIPNET's "compilation switch" strategy - which hopefully
will become command-line params at some point, but not yet. We need to doc the
possible switches somewhere...]

## SIPNET derivatives

**estimate**: The parameter estimation program for SIPNET. Its operation is
controlled by the file 'estimate.in'.

**sensTest**: [*ARCHIVED*] A program that tests the sensitivity of the model-data fit to
values of a parameter (essentially a one-dimensional parameter
estimation). Its operation is controlled by the file 'sensTest.in'.

## Stand-alone Utilities 

**subsetData**: A utility to subset a data record temporally. This utility
can also perform daily aggregation on the data if so desired. Its
operation is controlled by subset.in.

**transpose**: A little utility to transpose a text file that contains a
rectangular matrix of data. This may be necessary to read certain files
into Excel (e.g. the single-variable output files). Its usage is
'transpose filename'.

**bintotxt**, **txttobin**: Utilities to convert the hist file output from a
estimate run from a binary file to a text file, and back again.
