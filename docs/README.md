# SIPNET

SIPNET (Simplified Photosynthesis and Evapotranspiration Model) is an ecosystem model designed to efficiently simulate carbon and water dynamics. Originally developed for assimilation of eddy covariance flux data in forest ecosystems, current development is focused on representing carbon balance and GHG fluxes and agricultural management practices.

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.17148669.svg)](https://doi.org/10.5281/zenodo.17148669)


## Getting Started

### Requirements

- `gcc` or `clang` 
- `make`

### Quick Start

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

### Run Tests

- Unit tests:
  ```bash
  make unit
  # or: make testbuild && ./tools/run_unit_tests.sh
  ```
- Smoke tests:
  ```bash
  make smoke
  # or: ./tests/smoke/run_smoke.sh
  ```
- Full tests (build + unit + smoke):
  ```bash
  make test
  ```
- Clean up test artifacts:
  ```bash
  make testclean
```

## Documentation

Documentation for SIPNET is published at [pecanproject.github.io/sipnet](https://pecanproject.github.io/sipnet/), which is built using `mkdocs`. 
See the [Documentation section](CONTRIBUTING.md#documentation) in the CONTRIBUTING page for more information
about how to write and compile the documentation.

Key pages:
- [Model inputs](user-guide/model-inputs.md): Input files and configuration.
- [Model outputs](user-guide/model-outputs.md): Output files and structure.
- [Parameters](parameters.md): Model parameters and settings.

## Contributing

See the main [Contributing](CONTRIBUTING.md) and [Code of Conduct](CODE_OF_CONDUCT.md) pages for details on how to contribute to SIPNET.

## License

Distributed under the BSD 3-Clause license. See [LICENSE](../LICENSE) for more information.
