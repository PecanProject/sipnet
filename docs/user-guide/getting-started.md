# Getting Started with SIPNET

Use this guide to install the prerequisites, build the model, and run your first simulations and tests.

## Requirements

- `gcc` or `clang`
- `make`

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

## Run Tests

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
