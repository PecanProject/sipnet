# Changelog Guidelines for GitHub Copilot

When assisting with commit messages, pull requests, or changelog generation, adhere to the following `CHANGELOG.md` format and structure:

## Structure & Headings
- The file must be named `CHANGELOG.md` and use Markdown formatting.
- Use `## **SIPNET X.X.X - "<description>"**` for version headers, except for the top [Unreleased] section.

## Categories
All changes must be organized under the following exact subheadings:
### Added
- For new features.
### Fixed
- For any bug fixes.
### Changed
- For changes in existing functionality.
### Removed
- For removed features.

## Formatting Rules
- Never use issue numbers (e.g., #123) for changes.
- Always include the Pull Request number at the end of each line in parentheses, format: `(#PR_NUMBER)`.
- Example format: `- Add user authentication system (#452).`
