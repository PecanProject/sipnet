# SIPNET

SIPNET (Simplified Photosynthesis and Evapotranspiration Model) is an ecosystem model designed to efficiently simulate
carbon and water dynamics. Originally developed for assimilation of eddy covariance flux data in forest ecosystems, 
current development is focused on representing carbon balance and GHG fluxes and agricultural management practices.

## Quick Start

1. Clone the repository:
   ```bash
   git clone https://github.com/PecanProject/sipnet.git
   cd sipnet
   ```
2. Build the SIPNET executable:
   ```bash
    make
    ```
3. Run a test simulation:
   ```bash
   ./src/sipnet -i tests/smoke/sipnet.in
   ```
4. Check the output:
   ```bash
   cat tests/smoke/niwot.out
   ```

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
controlled by the file 'sipnet.in'; see that file for further configuration options.
[Updated description TBD]

## Documentation

Documentation for SIPNET is published at [pecanproject.github.io/sipnet](https://pecanproject.github.io/sipnet/), which is built using `mkdocs`. See 
the [Documentation section](CONTRIBUTING.md#documentation) in the CONTRIBUTING page for more information
about how to write and compile the documentation.

## Roadmap or Changelog section?

Do we want to list upcoming work? We could link to the issues page, and/or to the changelog

## Contributing

See the main [Contributing](CONTRIBUTING.md) page

## License

Distributed under the BSD 3-Clause license. See [LICENSE](https://github.com/PecanProject/sipnet/blob/master/LICENSE) for more information.
