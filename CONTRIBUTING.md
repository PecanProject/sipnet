
## Style Guide

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
