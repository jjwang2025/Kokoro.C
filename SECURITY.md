# Security Policy

## Supported Versions

`Kokoro.C` is currently maintained as a single active development line.

Security fixes are expected to land in the latest released version first.

## Reporting A Vulnerability

Please do not open a public issue for a suspected security problem.

Instead, report it privately to the repository maintainer with:

- a short description of the issue
- affected files or code paths
- reproduction steps if available
- impact assessment if known

Expected handling:

1. Acknowledgement of the report.
2. Reproduction and severity review.
3. Fix preparation.
4. Coordinated public disclosure after a fix is available, when appropriate.

## In Scope

Examples of relevant reports:

- unsafe file handling in CLI output paths
- malformed input causing crashes or undefined behavior
- dependency packaging issues that expose unexpected binaries
- release workflow mistakes that leak unintended files

## Out Of Scope

- model quality issues
- normal synthesis mistakes or pronunciation problems
- upstream vulnerabilities in third-party assets before they affect this repository's packaging or runtime integration
