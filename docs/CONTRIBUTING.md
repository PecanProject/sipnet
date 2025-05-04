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
2. Ensure that clang-format and clang-tidy are installed (and install them if running on MacOS)
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

###  Clang Format Guidelines

This project uses [`clang-format`](https://clang.llvm.org/docs/ClangFormat.html) to ensure a consistent coding style across all C/C++ source files. The format rules are defined in the `.clang-format` file at the root of the repository.

####  Install Clang Format

If `clang-format` is not already installed on your system, you can install it using:

- **macOS**  
  ```bash
  brew install clang-format
  ```

- **Ubuntu/Debian**  
  ```bash
  sudo apt install clang-format
  ```

- **Windows**  
  Install via [LLVM releases](https://releases.llvm.org/download.html) or with Chocolatey:  
  ```bash
  choco install llvm
  ```

####  How to Format Code

You can format individual files manually or auto-format all modified files before committing:

- Format a single file:
  ```bash
  clang-format -i path/to/file.c
  ```

- Format all C/C++ files in a directory:
  ```bash
  find src -name "*.c" -o -name "*.h" | xargs clang-format -i
  ```

- Format all staged changes:
  ```bash
  git clang-format
  ```

####  Tip: Use the Pre-Commit Hook

To avoid formatting-related CI failures, always run the setup script before your first commit:

```bash
tools/setup.sh
```
This installs a pre-commit hook that automatically checks formatting on staged C files.



### Clang Tools

This repo uses [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and [clang-tidy](https://clang.llvm.org/extra/clang-tidy/index.html)
to ensure code consistency and prevent (some) bad coding practices. Each tool has a configuration file in the repo root directory.

**clang-format** is run on each modified C-language file on commit; the commit will fail if any of the format checks fail.
To fix these errors:
* if all changes are staged (via `git add`), run the command `git clang-format` to fix the formatting and re-commit
* if not all changes are staged (likely a `git commit -a` command), either `git add` all the changes, or try `git clang-format -f` to reformat all modified files
* `clang-format` may also be run outside of `git`, as a means of updating a file's format regardless of modification status (see `clang-format --help`)

**clang-tidy** is one of the checks run on PR submission (along with building and testing) via the [cpp-linter](https://cpp-linter.github.io/cpp-linter-action/) 
github action. If this action fails, a comment will be made in the PR detailing the issues found. **clang-format** is 
also run by this action, so formatting issues that might have been bypassed on commit will be found here. You may attempt to have clang-tidy 
automatically fix clang-tidy issues with the command:<br>
```clang-tidy --fix <filename>```


## Documentation

What goes in **Doxygen**:
- Documentation for functions, classes, and parameters.

What goes in **docs/*md**:
- User guides and tutorials.
- Documentation of equations, theoretical basis, and parameters.

### Building the Documentation with `mkdocs`

Documentation is located at https://pecanproject.github.io/sipnet/, and can be rebuilt using `mkdocs`. A brief summary 
of use is listed here, or see the Getting Started page for `mkdocs` [here](https://www.mkdocs.org/getting-started/) for
more information. 

Issue the following command to install `mkdocs` and the third-party extensions usedin SIPNET:
```
pip install mkdocs mkdocs-material pymdown-extensions
```
The `material` theme can be found [here](https://github.com/squidfunk/mkdocs-material).

MkDocs comes with a built-in dev-server that lets you preview your documentation as you work on it. Make sure you're 
in the same directory as the mkdocs.yml configuration file, and then start the server by running the mkdocs serve 
command:

```
$ mkdocs serve
INFO    -  Building documentation...
INFO    -  Cleaning site directory
INFO    -  Documentation built in 0.22 seconds
INFO    -  [15:50:43] Watching paths for changes: 'docs', 'mkdocs.yml'
INFO    -  [15:50:43] Serving on http://127.0.0.1:8000/
```
Open up http://127.0.0.1:8000/ in your browser, and you'll see the documentation home page.

The dev-server also supports auto-reloading, and will rebuild your documentation whenever anything in the configuration
file, documentation directory, or theme directory changes.

If the structure of the documentation has changed (e.g., adding, moving, removing, or renaming pages), update `mkdocs.yml` in the root 
directory to reflect these changes and issue this command to rebuild:

```
mkdocs build
```

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