# SIPNET Source Code Documentation
<!--This README is also used as the main page for Doxygen-generated documentation.-->

This document provides an overview of the SIPNET model source code and utilities.  

## Organization and Packages

[Redo this once we finish code reduction changes]

Organization strategy:
* SIPNET and common files each have their own directory
* Stand-alone utilities all go into the `utilities` subdirectory

### Main program - SIPNET

[Add description here of SIPNET's "compilation switch" strategy - which hopefully
will become command-line params at some point, but not yet. We need to doc the
possible switches somewhere...]

### Stand-alone Utilities 

**subsetData**: A utility to subset a data record temporally. This utility
can also perform daily aggregation on the data if so desired. Its
operation is controlled by subset.in.

**transpose**: A little utility to transpose a text file that contains a
rectangular matrix of data. This may be necessary to read certain files
into Excel (e.g. the single-variable output files). Its usage is
'transpose filename'.
