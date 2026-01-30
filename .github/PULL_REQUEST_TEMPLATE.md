<!-- Please fill out the sections below to help reviewers. -->

## Summary

<<<<<<< HEAD
- **Motivation**: (Why are you making this change?)
- **What**: (Short description of the change)

## How to test

Steps to reproduce and verify the change locally (commands, expected output):

make
cd tests/smoke/<new-or-updated-test>
../../sipnet -i sipnet.in

## Related issues

- Fixes or relates to: #<issue>
=======
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
>>>>>>> ffdf0421f622091e702eadf686263ab7465ac6b9

## Checklist

- [ ] Tests added for new features
- [ ] Documentation updated (if applicable)
<<<<<<< HEAD
- [ ] Update CHANGELOG.md
- [ ] Requested review from at least one other developer


## Additional notes

If this PR is large or non-trivial, please summarize the design decisions and any alternatives considered. If you've already discussed this on Slack or an issue, link those conversations.
=======
- [ ] `docs/CHANGELOG.md` updated with noteworthy changes
- [ ] Code formatted with `clang-format` (run `git clang-format` if needed)
- [ ] Requested review from at least one CODEOWNER

**For model structure changes:**
- [ ] Removed `\fraktur` font formatting from `docs/model-structure.md` for implemented features

---

**Note**: See [CONTRIBUTING.md](../docs/CONTRIBUTING.md) for additional guidance. This repository uses automated formatting checks; if the pre-commit hook blocks your commit, run `git clang-format` to format staged changes.
>>>>>>> ffdf0421f622091e702eadf686263ab7465ac6b9
