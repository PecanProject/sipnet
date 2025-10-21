# Change Log

All notable changes are kept in this file. 
All changes made should be added to the section called `Unreleased`. 
Once a new release is made this file will be updated to create a new `Unreleased` section for the next release.

For more information about this file see also [Keep a Changelog](http://keepachangelog.com/) .

<!-- 
sections to include in release notes:

## [Unreleased]

### Added

- Add woodCreation to sipnet.out (#161)

### Fixed

### Changed

### Removed

### Git SHA
-->

## [Unreleased]

### Added

### Fixed

### Changed

### Removed

### Git SHA

## **SIPNET 2.0.0 - "SIPNET Overhaul"**

### Added

-Irrigation events (#26)
- Changelog (#33)
- Model structure and parameter documentation (#42)
- Build docs and push to gh-pages (#41)
- Add documentation website (pecanproject.github.io/sipnet) with automated builds (#53)
- Handle planting & harvest events(#55)
- events.out file for agronomic event handling (#57)
- Utility `tools/trim_first_chars.sh` to trim the first n characters from every row in a file, useful for updating old input files to remove location column
- Expanded smoke test cases to better cover SIPNET modeling options (#109, #114)
- Converted all compile-time switches not removed or hard-coded to be on into switches to run-time options (#114)
- Added extensive documentation in code listing the source(s) that the code implements (#135)
- Added python tool for comparing smoke test results with expected values (#142)

### Fixed

- Fixed OOM issue when reading bad data (#38, #45)
- Event order checks no longer only compare to first record (#74, #77)
- Fixed long-standing bug wherein microbePulseEff was not set to 0 when MICROBES was off (#114)
- Fixed long-standing bug with missing rSoil flux calc when LITTER_POOL is on (#139)
- Fixed long-standing bug with missing frozen soil check in soilBreakdown when LITTER_POOL is on (#140)

### Changed

- Reorganized codebase (#34, #37)
- Document & combine `calclighteff` functions (#35)
- Deprecated: "RUNTYPE" is obsolete. Will be silently ignored if set to 'standard' or error if set to anything else. Runs in 'standard' mode by default.
- Deprecated: "LOCATION" is obsolete. Will be ignored with warning. (#92)
- Deprecated: All columns in *.param except for name and value. Will be ignored with warning. (#92)
- Deprecated: location and soilWetness columns in input climate files. If both are present, will be ignored with warning. (#92, #127)
- Changed: sipnet.out columns will be zero when relevant modes are off (e.g., litter, microbes) (#143)
- Changed: reorganized code in sipnet to better model fluxes-then-pool-updates flow (#154)
- Changed: sipnet.out format; it no longer has loc, litterWater or fPAR columns (#155)

### Removed

- Removed many experimental sites, data, and executable code as part of reorg (#34, #37)
- Removed obsolete run types senstest and montecarlo and associated code (#69, #76)
- Removed obsolete estimate program and associated code (#70, #82)
- Removed multi-site support; in particular, output files no longer have a location column (#92)
- Removed or hard-codes 'on' many compile time switches (#114)
- Removed `SOIL_QUALITY` mode (#125)
- Removed `soilWetness` mode (#127)
- Removed obsolete model params; sipnet now warns on unknown params in the params file (#124)

### Git SHA
[TBD]

## **SIPNET 1.3.0 - "Event Handler"**

v1.3.0 represents the initial development of support for agricultural management events.

### Added 

- Introduced Event Handler infrastructure #23
- Add testing infrastructure.

### Changed

- Code cleanup (including comment standardization and spelling corrections).
- Update Doxyfile
- Add `make help`.

### Git SHA
8ff893e61d69d0374bdf0fa14d156fd621c40eb4

## **SIPNET 1.2.1 - "Add LICENSE and minor fixes"**

### Added

- Add BSD 3-Clause LICENSE file.
- Migrate documentation from Word to Markdown.

### Fixed

- Minor fixes prior to agricultural management implementation.
- Bug fix for output formatting (identified by @Qianyuxuan).

### Git SHA
0c77ce863ac61113740c759dbe502a74e2d64edf

## **SIPNET 1.2.0 - "fAPAR assimilation"**

### Added

- Modify fAPAR calculation to enable assimilation of MODIS satellite-derived fAPAR.

### Publications

Zobitz, J.M., David J.P. Moore, Tristan Quaife, Bobby H. Braswell, Andrew Bergeson, Jeremy A. Anthony, and Russell K. Monson. 2014. “Joint Data Assimilation of Satellite Reflectance and Net Ecosystem Exchange Data Constrains Ecosystem Carbon Fluxes at a High-Elevation Subalpine Forest.” Agricultural and Forest Meteorology 195–196 (September):73–88. https://doi.org/10.1016/j.agrformet.2014.04.011.

### Git SHA
97a225956775035506f573a29c7022de8d7d269d

## **SIPNET 1.1.0 - "Roots and Microbes"**

**Moore et al. (2008)**

- Support joint CO2 and H2O assimilation.

**Zobitz et al. (2008)**

- Add process-based soil respiration with microbes "soil quality model".
- Add fine and coarse root pools.
- Compare different model structures (base, soil quality, and roots).
- Calculate Transpiration using Ball Berry, initial implementation of and Penman-Monteith

### Publications

Moore, David J.P., Jia Hu, William J. Sacks, David S. Schimel, and Russell K. Monson. 2008. “Estimating Transpiration and the Sensitivity of Carbon Uptake to Water Availability in a Subalpine Forest Using a Simple Ecosystem Process Model Informed by Measured Net CO2 and H2O Fluxes.” Agricultural and Forest Meteorology 148 (10): 1467–77. https://doi.org/10.1016/j.agrformet.2008.04.013.

Zobitz, J. M., D. J. P. Moore, W. J. Sacks, R. K. Monson, D. R. Bowling, and D. S. Schimel. 2008. “Integration of Process-Based Soil Respiration Models with Whole-Ecosystem CO2 Measurements.” Ecosystems 11 (2): 250–69. https://doi.org/10.1007/s10021-007-9120-1.

## **SIPNET 1.0.0 - "SIPNET First Release"**

The first release of SIPNET reflected a series of improvements over the original SIPNET model (Braswell et al., 2005).
The model was developed by Bill Sacks and Dave Moore, with contributions from John Zobitz. The model was parameterized using data from the Harvard Forest flux tower using MCMC (Sacks et al., 2006).
The model was designed to simulate the carbon and water cycles of a forest ecosystem at half-daily time steps.


**Braswell et al. (2005)**

- Initial version of SIPNET.
- Half-daily time step with two vegetation carbon pools and one soil pool.
- MCMC parameter fitting with observations from Harvard forest flux tower.

**Sacks et al. (2006)**

- More complex water routine incorporating evaporation and snow pack.
- Evergreen leaf phenology.

**Sacks et al. (2007): "Better Respiration"**

- Shut down photosynthesis and foliar respiration when soil temperature < threshold.
- Partition autotrophic and heterotrophic respiration.

### Git SHA
47d6546e245384dbda7e981a3bc8b729d4f756fc

### Publications

Braswell, Bobby H., William J. Sacks, Ernst Linder, and David S. Schimel. 2005. “Estimating Diurnal to Annual Ecosystem Parameters by Synthesis of a Carbon Flux Model with Eddy Covariance Net Ecosystem Exchange Observations.” Global Change Biology 11 (2): 335–55. https://doi.org/10.1111/j.1365-2486.2005.00897.x.

Sacks, William J., David S. Schimel, and Russell K. Monson. 2007. “Coupling between Carbon Cycling and Climate in a High-Elevation, Subalpine Forest: A Model-Data Fusion Analysis.” Oecologia 151 (1): 54–68. https://doi.org/10.1007/s00442-006-0565-2.

Sacks, William J., David S. Schimel, Russell K. Monson, and Bobby H. Braswell. 2006. “Model‐data Synthesis of Diurnal and Seasonal CO2 Fluxes at Niwot Ridge, Colorado.” Global Change Biology 12 (2): 240–59. https://doi.org/10.1111/j.1365-2486.2005.01059.x.
