# SIPNET



SIPNET (Simplified Photosynthesis and Evapotranspiration Model) is an ecosystem model designed to efficiently simulate carbon and water dynamics. Originally developed for assimilation of eddy covariance flux data in forest ecosystems, current development is focused on representing carbon balance and GHG fluxes and agricultural management practices.

## Getting Started

### Installing

[What's required?]

[Python may be required in the future, depending on what happens with formatting pre-commit hooks; we'll see]

[Steps for building the program with make; mention 'make help' here too]

[Add reference here to src/README.md for other utilities in this package]

### Executing the Program

[Running the program - example]

[Description of sipnet.in and different modes, some variation of the below paragraph]

The program used for forward runs of the model. Its operation is
controlled by the file 'sipnet.in'. SIPNET can run in three different
modes: 'standard', for a standard forward run; 'senstest', for
performing a sensitivity test on a single parameter; and 'montecarlo',
for running the model forward using an ensemble of parameter sets. See
'sipnet.in' for further configuration options for each of these types of
runs.

## Documentation

[Describe building the doxygen documentation and what it contains]

## Contributing

See the main [Contributing](/CONTRIBUTING.md) page
