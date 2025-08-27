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
3. Change directory and run a test simulation:
   ```bash
   cd tests/smoke/niwot
   ../../../sipnet -i sipnet.in
   ```
4. Check the output:
   ```bash
   cat niwot.out
   ```

## Getting Started

### Installing

See the quickstart above.

### Executing the Program

[Link to cli docs, when they get written]

[Link to description of all the input files]

### Running Tests
#### Smoke Tests
From the root sipnet directory, run:
```bash
./tests/smoke/run_smoke.sh
```
The end of the output from that script should be:
```shell
=======================
SUMMARY:
Skipped directories: 0
SIPNET OUTPUT:
Passed:  5/5
Failed:  0
EVENT OUTPUT:
Passed:  5/5
Failed:  0
CONFIG OUTPUT:
Passed:  5/5
Failed:  0
=======================
```
#### Unit Tests
Build the unit tests:
```bash
make test
```
Run the unit tests:
```bash
make testrun
```
[TODO: create a wrapper around `make testrun` that will parse the output
and report results in an easier format]

## Documentation

Documentation for SIPNET is published at [pecanproject.github.io/sipnet](https://pecanproject.github.io/sipnet/), which is built using `mkdocs`. See 
the [Documentation section](CONTRIBUTING.md#documentation) in the CONTRIBUTING page for more information
about how to write and compile the documentation.

## Contributing

See the main [Contributing](CONTRIBUTING.md) page

## License

Distributed under the BSD 3-Clause license. See [LICENSE](https://github.com/PecanProject/sipnet/blob/master/LICENSE) for more information.
