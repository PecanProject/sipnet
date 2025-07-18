# SIPNET Smoke Tests

Smoke tests run as part of the build process in github's CI workflow. Thus, they should be kept short (e.g., run time
less than a minute as a rough guideline). This document describes the current smoke tests.

## Tests Files

Each smoke test contains the following files:

**INPUT**
* `events.in`: events input file (may be empty if no events, as with the Niwot case)
* `sipnet.clim`: climate input file
* `sipnet.in`: sipnet configuration file
* `sipnet.param`: model parameter input file

**OUTPUT**
* `events.exp`: expected events output file, compared to generated `events.out`
* `sipnet.exp`: expected sipnet output, compared to generated `sipnet.out`

## Niwot

The "original" smoke test, this is a simple run at Niwot Ridge(?), using out-of-the-box settings for SIPNET.

## Russell Ranch

There are four runs at a site ~8.5 km NNE of [Russell Ranch](https://russellranch.ucdavis.edu/).
These runs attempt to span the various modeling options available in SIPNET, as indicated in the table below.

NOTE: Run 2 has been disabled due to removal of multipool support.

| Param                  | Requirements  | Run 1 <br>(default) | Run 2 | Run 3 | Run 4 |
|------------------------|---------------|---------------------|-------|-------|-------|
| GROWTH_RESP            |               | 0                   | 0     | 1     | 0     |
| WATER_HRESP            |               | 1                   | 1     | 0     | 1     |            
| LEAF_WATER             |               | 0                   | 0     | 1     | 0     |             
| SNOW                   |               | 1                   | 1     | 1     | 0     |            
| GDD                    | !SOIL_PHENOL  | 1                   | 1     | 1     | 0     | 
| SOIL_PHENOL            |  !GDD         | 0                   | 0     | 0     | 1     |        
| NUM_SOIL_CARBON_POOLS  |               | 1                   | 3     | 1     | 1     |
| LITTER_POOL            |  !MULTI_POOL  | 0                   | 0     | 1     | 0     | 
| MICROBES               | !MULTI_POOL   | 0                   | 0     | 0     | 1     |

where MULTI_POOL refers to NUM_SOIL_CARBON_POOLS > 1.
