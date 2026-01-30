---
name: Pull Request
title: "<issue number> <Concise description of the proposed change>"
---
<!-- Please fill out the sections below to help reviewers. -->

## Summary

- **Motivation**: Why is this change needed?
- **What**: Short description of the functional or documentation changes

## How to test

Steps to reproduce and verify the change locally:

```bash
make
cd tests/smoke/<new-or-updated-test> ../../sipnet -i sipnet.in
```

## Related issues

- Fixes #<issue> (or "Relates to #N" if this is not a resolution of that ticket)

## Checklist

- [ ] Tests added for new features
- [ ] Documentation updated (if applicable)
- [ ] `docs/CHANGELOG.md` updated with noteworthy changes
- [ ] Code formatted with `clang-format` (run `git clang-format` if needed)
- [ ] Requested review from at least one CODEOWNER

**For model structure changes:**
- [ ] Removed `\fraktur` font formatting from `docs/model-structure.md` for implemented features

---

**Note**: See [CONTRIBUTING.md](../docs/CONTRIBUTING.md) for additional guidance. This repository uses automated formatting checks; if the pre-commit hook blocks your commit, run `git clang-format` to format staged changes.

