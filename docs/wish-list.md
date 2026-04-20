# SIPNET Wish List

This is the spot to capture ideas on future work that have not been fully thought out (and should thus go into a 
feature request).

Order is not relevant here, though we _could_ try to order by priority, I suppose. Seems like that will get out of date
very easily, though. Also, the infrastructure vs Modeling divide is loose at besst.

## Infrastructure Ideas

### More doxygen docs
We have doxygen set up, and we even have it auto publish, but... we don't have a lot of actual documentation. This is 
something that could (and should) be incrementally improved as we work on the code, but it might be a really good idea
spend some focused time on this.

### Remove unsupported code
There are a lot of compiler options and related code that we have no intention of supporting, and we are not even sure 
what still works.

We should decide what options we want to keep, remove code for those we don't, and make sure remaining all works 
together (where it should).

### Revamp `sipnet.in` 

The input config file `sipnet.in` probably has cruft in it, and/or might be a bit of a relic. We should take a good look
there and at least make sure it is all still relevant. Even more so, we should think about how we want to handle this
file in the scope of the next item (see below, compile flag to cli option conversion).

### Compile flag to cli option conversion

This is a biggie, and possibly the highest priority here. We should replace the current compile-flag strategy with 
command-line interface options _for the options we are choosing to keep_. Assuming we put the same options in sipnet.in,
we should have any cli-specified options have priority (which is typical).

This project has overlap with both "Revamp sipnet.in" and "Remove unsupported code".

### Replace use of `exit` with something more testable
SIPNET's strategy of calling `exit()` when it encounters an error is difficult to test, requiring patching in a custom
exit handler. Investigate and find a replacement suitable for the C language. Note: this may not be worth the effort.

### Revamp unit testing (requires command line option conversion, and `exit` replacement if that is going to happen)
Once we have moved away from the compiler-switch strategy (and the use of `exit`, if that happens), we can more easily 
use a standard unit testing package (such as [CUnit](https://cunit.sourceforge.net/index.html), though there are others). 
This would make creating more unit tests easier.

### More unit testing!
ESPECIALLY after the Revamp unit testing item, fill out our unit test coverage. Note that this is best for the 
infrastructure parts of the code; the algorithmic/modeling parts are likely best tested at the integration level. Also, 
the goal is never to get 100% coverage, IMO - that just leads to bad tests.

### More integration/smoke testing!
Our 'integration' testing right now consists of one simple regression test. This can and should be more extensive. 

### Add logging
There are a lot of details of SIPNET's runs that are not reported. We should create a logging mechanism, convert 
existing `printf` statements to use that system, and then add a lot more. Some items we may want to add:
* configuration options in effect
* version of SIPNET
* warnings and errors (possibly all already there?)
* information flow description

We might also want to have a verbosity option to control how much gets reported.

## Modeling Ideas

### Features excluded in favor of simplicity

May be revisited if motivated by specific use cases or to improve model skill

If to improve model skill, improvements should be demonstrated under multiple conditions.

* Multi-layer soil  
* Rooting zone differences by crop type  
* Sub-daily irrigation  
* Represent subdaily irrigation as duration as well as amount.  
* Additional Q10   
* Split mineral N into NO3 and NH4 and explicitly represent nitrification and denitrification  
* CH4 flux: account for diffusion, methane oxidation, ebullition, and plant transport  
* Methanotrophy  
* N immobillization  
* Variable Plant CN Ratios  
* Water holding capacity of SOM  
* Fruit, nut, seed pools  
* Orchard floor preparation practices  
* Clay / Sand  
* Lignin  
* pH


## Lesser Ideas (in either scope or importance)

### Have events work with ROOTS=0 (unless we remove that case!)
[Only if motivated by specific use case] The event handler requires `ROOTS` to be on; this is apparent in event modeling that 
distinguishes between wood C, fine root C, and coarse root C (in addition to leaf C).
Those three pools are (one or two?) when `ROOTS=0`. We punted on enabling event handling for
that case.
