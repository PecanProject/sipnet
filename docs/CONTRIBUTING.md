# Contributing to SIPNET

We welcome contributions to SIPNET. This document outlines the process for contributing to the SIPNET project.

All contributors are expected to adhere to the PEcAn Project [Code of Conduct](https://github.com/PecanProject/pecan/blob/develop/CODE_OF_CONDUCT.md).

## Setup

1. Clone this repository:

   ```bash
   git clone git@github.com:PecanProject/sipnet
   ```

2. Run the setup script (once per clone):

   ```bash
   tools/setup.sh
   ```

The `tools/setup.sh` script verifies that Python ≥ 3.8 is available,
ensures that `clang-format`, `clang-tidy`, and `git clang-format` are
installed (it automatically installs them on macOS and prints installation
instructions for Ubuntu/Debian), and then copies the clang pre‑commit hook into
`.git/hooks` where it will check formatting on every commit; if issues are found it
aborts the commit so you can run `git clang-format` and re‑stage the
changes.

*Note – running the script is unnecessary for documentation‑only edits,
but it will save you time whenever you touch C/C++ code.*

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
  - For new model features related to the structure, remove relevant `\fraktur` font formatting from `docs/model-structure.md` to indicate that the feature has been implemented.

Pull requests must pass all required checks to be merged into master, including the code format and style checks described below.
  
## Code Format & Style

We follow the standard LLVM/Clang formatting rules. Formatting is automatic, and you rarely have to think about them:

1. **Run the setup script once** (see *Setup* above).  
   It installs a **pre‑commit hook** that blocks any commit whose C/C++
   files are not already formatted.

2. **If the hook stops your commit**, run:

   ```bash
   # format only what you just staged
   git clang-format
   # restage fixes and commit again
   git add -u
   git commit
   ```
If `git clang-format` fails because not all changes are staged (likely a `git commit -a` command), either `git add` all the changes, or try `git clang-format -f` to reformat all modified files
3. **clang‑tidy static analysis** runs automatically in CI.  
    If the bot leaves comments, adjust your code as instructed and push.

    To run `clang-tidy` manually before committing or pushing changes, use the following command from the project root:
    
    ```bash
    # Replace <path/to/filename.c> with the file you want to check
    clang-tidy <path/to/filename.c>
    ```
    
The hook and CI will tell you what to fix.

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
  - Name version in `docs/CHANGELOG.md`
- Include content from `docs/CHANGELOG.md` file in release description.
- Attach Mac and Linux compiled SIPNET binaries to release.
- Update `PROJECT_NUMBER` in `docs/Doxyfile`
