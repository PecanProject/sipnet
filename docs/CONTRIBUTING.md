# Contributing to SIPNET

We welcome contributions. This page is the one‑stop guide for developers.

All contributors must follow the project [Code of Conduct](CODE_OF_CONDUCT.md).

## Developer Quickstart

New contributors are encouraged to start with [good first issues](../issues?q=is%3Aissue%20state%3Aopen%20label%3A%22good%20first%20issue%22).

For a short quickstart and how to get the site and examples running, see the user guide: https://pecanproject.github.io/sipnet/#getting-started
## GitHub Workflow

### Branches

The `master` branch is the default branch for SIPNET. Development should be done in feature branches. Feature branches should be named to clearly indicate the purpose, and may be combined with an associated issue, e.g. `ISSUE#-feature-name`.

### Pull Requests

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

### Propose and Receive Feedback

Before opening a pull request (PR), express your interest and get feedback by:

- Asking to be assigned to an existing issue (comment on the issue and request assignment).
- If the idea has not been proposed, or you want additional feedback, discuss it in the `#pecan` Slack channel.
- Optional: if the task is complex and no issue exists, open a new issue summarizing the discussion, including motivation and a proposed solution.
- Begin implementation once you have a thumbs-up (review/assignment) from a core maintainer (for example, `@Alomir` or `@dlebauer`).

This helps avoid duplicate work and ensures contributors receive early feedback on scope and design.

### PR expectations

When opening a PR, include the following in your description:

- **Motivation**: why the change is needed.
- **What changed**: short summary of the functional or documentation changes.
- **How to test**: steps to reproduce and verify the change (including commands if applicable).
- **Related issues**: link the issue(s) that motivated the change (use `Fixes #123` when appropriate).
- **Checklist**: ensure the PR includes tests (if code changes), documentation updates (if applicable), and an entry in `docs/CHANGELOG.md` for noteworthy changes.

Prior to merging a PR, it must:
- Include tests for new features
- Include documentation updates to reflect changes to functionality.
- Update CHANGELOG with any user-visible changes.
- Pass tests.
- Be approved by at least one maintainer.
  
## Code Format & Style

We follow the standard LLVM/Clang formatting rules. Formatting is automated with a pre-commit hook, so you wil rarely have to think about them.

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
