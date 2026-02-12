# Contributing to SIPNET

We welcome contributions. This page is the one‑stop guide for developers.

All contributors must follow the project [Code of Conduct](CODE_OF_CONDUCT.md).

## Developer Quickstart

- Clone the repository:
   ```bash
   git clone git@github.com:PecanProject/sipnet
   cd sipnet
   ```
- Setup (once per clone, described below):
  ```bash
  tools/setup.sh
  ```
- Build:
  ```bash
  make
  ```
- Run a sample:
  ```bash
  cd tests/smoke/niwot
  ../../../sipnet -i sipnet.in
  ```

**Setup Script** The `tools/setup.sh` script verifies that Python ≥ 3.8 is available and that `clang-format`, `clang-tidy`, and `git clang-format` are installed. Automatically installs `clang` tools on macOS and prints installation instructions for Ubuntu/Debian. Then copies the clang pre‑commit hook into `.git/hooks` so that code formatting is checked on every commit.

_Note – running `tools/setup.sh`  is not necessary for documentation‑only edits,
but it will save you time whenever you touch C/C++ code._
 
New contributors are encouraged to start with [good first issues](https://github.com/PecanProject/sipnet/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22).

For a short quickstart and how to get the site and examples running, see the user guide: [Getting Started](user-guide/getting-started.md)

## GitHub Workflow

### Propose and Receive Feedback {#propose-and-receive-feedback}

**Before starting work on a proposed change, get approval first.** 

- For an existing (ticketed) issue, ask to be assigned to it (comment on the issue and request assignment).
- If you want additional feedback, discuss it in the `#pecan` Slack channel.
- If no issue exists for the proposed work, create a new issue summarizing the work, including motivation and a
  proposed solution.
- The issue will be reviewed by one or more core maintainers, possibly with requests for clarification and/or 
  suggestions for changes.
- If the issue is approved, it will be assigned (likely to the creator).
- Once a ticket is assigned (and only then) should implementation begin.

This helps avoid duplicate work and ensures contributors receive early feedback on scope and design.

Exceptions (no issue required):
- Maintainers may submit PRs without related tickets, based on internal team communication.
- Trivial changes to documentation and comments (e.g. typo fixes, clarifications).
- Smaller changes may be submitted with maintainer approval via Slack or email. This discussion should be summarized in
  the PR.

Note: no changes to actual code are considered trivial.

**PRs submitted outside the above process may not be reviewed.**

### Branches

The `master` branch is the default branch for SIPNET. Development should be done in feature branches. Feature branches 
should be named to clearly indicate the purpose, and should start the number of its associated issue, e.g. 
`ISSUE#-feature-name`. Development may also be done in a fork of the repo.

### Pull Requests

As stated above, pull requests should be made from feature branches to the `master` branch, or a repo fork. Pull 
request titles should be of the form: `[<ISSUE#>] <Brief description of change>`, such as "[123] Add nitrogen balance check"
or perhaps "SIP321 Update cli help text" if you prefer that form.

**This repository has a PR template**; when opening a PR, make sure to fill out the template as indicated.

**Prior to merging a PR, it must:**
- Pass all unit and integration tests
- Be approved by at least one CODEOWNER
- Include updates and additions to:
  - Tests (if code changes)
  - Documentation (if applicable)
  - `docs/CHANGELOG.md` for noteworthy changes

All required checks must pass before merging, including the code format and style checks described below.

## Code Format & Style

We follow the standard LLVM/Clang formatting rules. Formatting is automated with a pre-commit hook, so you wil rarely have to think about them.

To set up formatting and static analysis checks:

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

If a commit is blocked, format staged changes:
  ```bash
  git clang-format
  git add -u && git commit
  ```
  To reformat all modified files: `git clang-format -f`.
- Optional local static analysis:
  ```bash
  clang-tidy path/to/file.c
  ```


## Documentation

- Build API docs (Doxygen) and site (MkDocs):
  ```bash
  make document
  ```
- Live preview while editing docs:
  ```bash
  pip install mkdocs mkdocs-material pymdown-extensions
  mkdocs serve
  # open http://127.0.0.1:8000/
  ```
  Update `mkdocs.yml` if you add/move pages.

### Using anchors for stable cross-references

When creating or updating documentation links, prefer explicit heading anchors so references remain stable when headings change slightly. We use the `attr_list` MkDocs extension to allow adding anchors directly to headings. To add an anchor, append `{#id}` to the heading, for example:

```
## Notation {#sec-notation}
```

Then link to that section from other pages using the file path plus `#id`, for example:

```
[Notation](parameters.md#sec-notation)
```

This makes cross-references robust to small edits in heading text and reduces broken links during refactors. Ensure `attr_list` is enabled in `mkdocs.yml` (it is enabled in this repo).

### Compiling Documentation

- Build API docs (Doxygen) and site (MkDocs):
  ```bash
  make document
  ```
  Outputs: `docs/api/` and `site/`.
- Live preview while editing docs:
  ```bash
  pip install mkdocs mkdocs-material pymdown-extensions
  mkdocs serve
  # open http://127.0.0.1:8000/
  ```
  Update `mkdocs.yml` if you add/move pages.

## Build

```bash
make
```

- See the project [README.md](README.md) for quick start. Use `make help` for all targets.


## Testing

New features require tests.

- Unit tests:
  ```bash
  make unit
  # or: make testbuild && ./tools/run_unit_tests.sh
  ```
- Smoke tests:
  ```bash
  make smoke
  ```
- Full check (unit + smoke):
  ```bash
  make test
  ```

- Clean up unit test artifacts:
  ```bash
  make testclean
  ```

Per‑suite runners live under `tests/sipnet/*`:
```bash
make -C tests/sipnet/test_events_infrastructure run
```

## Releases

- Use [Semantic Versioning v2](https://semver.org/) for SIPNET releases. 
- Update versions in:
  - `CITATION.cff`
  - `src/sipnet/version.h`
  - `docs/CHANGELOG.md`
  - `docs/Doxyfile` (`PROJECT_NUMBER`)
- Run tests (`make test`).
- Tag the git commit associated with the release `vX.Y.Z` and push tag to master.
    ```
    git tag v<X.Y.Z>
    ```
- Push tag to master branch, this will trigger a draft release
    ```
    git push origin v0.0.0-test
    ```
- Open draft on https://github.com/pecanProject/sipnet/releases.
- Add content from `docs/CHANGELOG.md` to release notes.
- Publish the GitHub release; this will trigger archiving on Zenodo
- Get doi for this release from Zenodo and add to release notes. (note: README has a doi that represents all versions, and resolves to latest)
