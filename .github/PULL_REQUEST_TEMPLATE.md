<!-- Please fill out the sections below to help reviewers. -->

## Summary

- **What**: Short description of the functional or documentation changes
- **Motivation**: Why is this change needed?

## How was this change tested?
List steps taken to test change, with appropriate outputs is applicable

If changes are made to any `sipnet.out` files in the `test/smoke` subdirectories, then include output from
`tools/smoke_check.py` by running the commands below and pasting the output at the end of this PR. Note that 
this must be done BEFORE submitting changes to any sipnet.out files.
```bash
make smoke
python tools/smoke_check.py run verbose <list test directories with changed outputs>
```

## Reproduction steps

If appropriate, list steps to reproduce the change locally

## Related issues

- Fixes #<issue> (or "Relates to #<issue>" if this is not a resolution of that ticket)

## Checklist

- [ ] PR title has the issue number in it ("[#123] <concise description of the proposed change>")
- [ ] Related issues are listed above with the form "Fixes #<issue>"
- [ ] Tests added for new features
- [ ] Documentation updated (if applicable)
- [ ] `docs/CHANGELOG.md` updated with noteworthy changes
- [ ] Code formatted with `clang-format` (run `git clang-format` if needed)

**For model structure changes:**
- [ ] Removed `\fraktur` font formatting from `docs/model-structure.md` for implemented features

---

**Note**: See [CONTRIBUTING.md](../docs/CONTRIBUTING.md) for additional guidance. This repository uses automated formatting checks; if the pre-commit hook blocks your commit, run `git clang-format` to format staged changes.
