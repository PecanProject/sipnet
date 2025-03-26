
## Style Guide

TODO:
1. add some 'code of conduct' lines here, or a separate file?
2. add info about our use of `clang-format` and `clang-tidy` once that has settled down
3. Add section about actually contributing - that is, git cloning, PRs, etc. Pull from David's nice doc he wrote for CARB.
    1. In fact, we might want to pull a LOT from that doc and put it here. I think it covers code of conduct, too.
4. Move the compiling section to more mainstream docs, as that is relevant to non-contributing users

## Compiling

SIPNET uses `make` to build the model and documentation. There are also miscellaneous targets for running analysis workflows:

```sh
# build SIPNET executable
make sipnet
# build documentation
make document
# clean up build artifacts
make clean
# list all make commands
make help
```
## Testing

SIPNET also uses `make` to build and run its unit tests. This can be done with the following commands:
```shell
# Compile tests
make test
# Run tests
make testrun
# Clean after tests are run
make testclean
```

If changes are made to the `modelStructure.h` file and unit tests are failing, try running the update script as shown below. Consider running this script even if unit tests _are not_ failing.
```shell
# Run this command from the root directory to update unit test versions of modelStructures.h
tests/update_model_structures.sh
```
