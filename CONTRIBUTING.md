# Contributing to SIPNET

We welcome contributions to SIPNET. This document outlines the process for contributing to the SIPNET project.

All contributors are expected to adhere to the PEcAn Project [Code of Conduct](https://github.com/PecanProject/pecan/blob/develop/CODE_OF_CONDUCT.md).

## Setup

If changes are intended for any C-code files, please execute this command:
```shell
tools/setup.sh
```
to run the setup script, which will:
1. Check to make sure that python is installed and is at least version 3.8
2. Ensure that clang-format and clang-tidy are installed (and install them if not on MacOS)
3. Install the pre-commit hook, which checks the format of modified C files. This will help prevent PR failures due to formatting issues by finding those issues earlier.

Note: This step is not necessary for changes to documentation, markdown files, config, etc.

## GitHub Workflow

#### Branches

The `master` branch is the default branch for SIPNET. Development should be done in feature branches. Feature branches should be named to clearly indicate the purpose, and may be combined with an associated issue, e.g. `ISSUE#-feature-name`.

#### Pull Requests

Pull requests should be made from feature branches to the `master` branch. 

Pull request descriptions should include a brief summary of the changes and links to related issues. 

Expectations for merging:
- Pass all unit and integration tests. 
- Approved by at least one other developer before being merged.
- Include updates and additions to 
  - Documentation
  - Tests
  - CHANGELOG.md 

Pull requests must pass all required checks to be merged into master, including [code format and style](#code-format-and-style) checks.
  
## Code Format and Style

### Clang Tools

This repo uses [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and [clang-tidy](https://clang.llvm.org/extra/clang-tidy/index.html)
to ensure code consistency and prevent (some) bad coding practices. Each tool has a configuration file in the repo root directory.

**clang-format** is run on each modified C-language file on commit; the commit will fail if any of the format checks fail.
To fix these errors:
* if all changes are staged (via `git add`), run the command `git clang-format` to fix the formatting and re-commit
* if not all changes are staged (likely a `git commit -a` command), either `git add` all the changes, or try `git clang-format -f` to reformat all modified files

## Documentation

What goes in **Doxygen**:
- Documentation for functions, classes, and parameters.

What goes in **docs/*md**:
- User guides and tutorials.
- Documentation of equations, theoretical basis, and parameters.

## Compiling SIPNET binaries

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

Any new features (that are worth keeping!) should be covered by tests.

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

## Releases

- Use [Semantic Versioning v2](https://semver.org/) for SIPNET releases.
- Tag releases with the version number `vX.Y.Z`.
- Include content from `CHANGELOG.md` file.
- Add compiled SIPNET binaries.