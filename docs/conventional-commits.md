# Commit message conventions

This repository follows [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/).
Executable rules are defined in
[`commitlint.config.mjs`](../commitlint.config.mjs), which extends
`@commitlint/config-conventional`.

All commit messages must be in English, including the subject, body and footer
prose. Documentation, source comments, Doxygen and examples must also be in English.
Commitlint validates syntax; language compliance is checked during review, not
automatically inferred from the message.

## Subject format

```text
type(scope)!: description
```

- `type` is required, lowercase and selected from the types below.
- `(scope)` is optional and lowercase. It identifies the affected area.
- `!` is optional and signals a breaking change.
- The separator is `: `, with one space after the colon.
- The description is required, in English, without a trailing period.
  Its letter case is not restricted.
- The complete subject is a single line of at most **100 characters**.

Example scopes: `observable`, `single`, `maybe`, `completable`, `disposable`,
`schedulers`, `cmake`, `deps`, `tests`, `examples`, `docs`. This list is illustrative,
not restrictive.

## Allowed types

| Type | Use in this repository | Example |
| --- | --- | --- |
| `feat` | New functionality or public API extension | `feat(observable): add a distinct operator` |
| `fix` | Incorrect behavior correction | `fix(maybe): terminate after the first success` |
| `docs` | Documentation, README and Doxygen comments | `docs(single): clarify the fromCallable contract` |
| `style` | Formatting without behavior changes | `style: group includes by origin` |
| `refactor` | Restructuring without added functionality or behavior fixes | `refactor: name template constraints` |
| `perf` | Performance improvement | `perf(observable): reduce value copies` |
| `test` | Added or updated tests | `test(completable): cover failing actions` |
| `build` | CMake, presets, vcpkg, dependencies and packaging | `build(cmake): add the Profile preset` |
| `ci` | Continuous integration workflows and checks | `ci: validate commit messages` |
| `chore` | Maintenance outside the other categories | `chore: update git exclusions` |
| `revert` | Reverting a previous change | `revert: undo the scheduler change` |

Choose the type matching the main intent. A dependency update belongs under
`build(deps)`, a test change under `test`, and a pipeline change under `ci`,
rather than classifying everything as `chore`.

## Body, breaking changes and trailers

The body and footer are optional and separated from the subject by a blank
line. Keep their lines within 100 characters; the conventional preset
allows exceptions, notably for URLs.

A breaking change can be indicated by `!` in the subject or by a
`BREAKING CHANGE:` footer explaining migration:

```text
feat(single)!: change the subscribe signature

BREAKING CHANGE: explicitly provide the onError callback for every subscription
```

Git trailers, including `Co-authored-by:`, remain allowed after a blank line.
The `revert` type must also follow the subject format; replace Git's automatic
`Revert "..."` subject.

Rejected subjects:

```text
Add tests
feature: add an operator
fix(observable):correct cardinality
docs:
ci: update the workflow.
```

## CI validation

The **Commit messages** job in the [CI workflow](../.github/workflows/ci.yml) uses
the commitlint CLI and conventional preset, pinned to **19.8.1**,
with the repository configuration:

- For a pull request targeting `develop` or `master`, it checks all commits
  in the pull request, not only the last commit or the PR title.
- For a push to `develop` or `master`, it checks the range between the old and
  new SHA. When creating a branch, it checks commits introduced relative
  to the other remote branches.
- Ranges are read directly from Git, without an API pagination limit.
- Empty messages, commitlint errors and warnings fail the job.
- No implicit exemptions: merge, revert and version subjects
  must also comply. For example, use `chore: merge develop`
  instead of an automatic `Merge ...` subject.

Existing history is not rewritten. Older commits are not checked
retroactively unless they belong to the event's commit range.
When squashing, also give the resulting commit a compliant subject.

To block noncompliant merges, make **Commit messages** a required
check in the GitHub branch protection rules for `develop` and `master`.
Configure this protection in repository settings, not in the workflow.
