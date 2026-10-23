# Project structure

```text
ReactivePlusPlusWrapper/
|-- .github/
|   `-- workflows/
|       |-- ci.yml
|       `-- cd.yml
|-- cmake/
|   `-- ReactivePlusPlusWrapperConfig.cmake.in
|-- docs/
|   |-- conventional-commits.md
|   `-- structure.md
|-- examples/
|   |-- CMakeLists.txt
|   |-- *.cpp                     (reactive-type and scheduling examples)
|   `-- consumer/
|       |-- CMakeLists.txt
|       `-- *.cpp                 (installed-package consumer)
|-- include/
|   `-- rppw/
|       |-- *.h                   (public C++ headers)
|       |-- *.inl                 (template and inline implementations)
|       `-- detail/
|           |-- Fwd.h
|           `-- Operators.inl
|-- resources/
|   `-- doxygen/
|       |-- Doxyfile
|       |-- index.md
|       `-- doxygen-awesome-css/  (third-party Git submodule)
|-- src/
|   `-- *.cpp                     (compiled library implementations)
|-- tests/
|   |-- CMakeLists.txt
|   |-- *Tests.cpp                (unit and scheduler integration tests)
|   |-- consumer/
|   |   |-- ConsumerApi.h
|   |   `-- *.cpp                 (multi-translation-unit API checks)
|   `-- support/
|       `-- *.h                   (shared test fixtures and helpers)
|-- .clang-format
|-- .gitignore
|-- .gitmodules
|-- CMakeLists.txt
|-- CMakePresets.json
|-- commitlint.config.mjs
|-- LICENSE
|-- README.md
|-- THIRD_PARTY_NOTICES
|-- vcpkg.json
```

The tree groups files with the same role instead of listing every header,
implementation, example or test. It describes the maintained project layout;
generated build contents and third-party submodule internals are not expanded.

## Root files

| File | Purpose |
| --- | --- |
| [`CMakeLists.txt`](../CMakeLists.txt) | Defines the C++20 static library and its namespaced CMake target, finds ReactivePlusPlus and threading dependencies, enables optional tests and examples, and configures installation and package exports. |
| [`CMakePresets.json`](../CMakePresets.json) | Provides Ninja-based configure, build and test presets for Debug, Release and Profile. Integrates the vcpkg toolchain, enables development tests and examples, and places generated files under `build/<preset>`. |
| [`vcpkg.json`](../vcpkg.json) | Declares package metadata, a pinned vcpkg baseline, the ReactivePlusPlus dependency and the optional `tests` feature supplying GoogleTest/GoogleMock. |
| [`README.md`](../README.md) | Main project guide: API overview, behavior contracts, usage, setup and build instructions. |
| [`LICENSE`](../LICENSE) | MIT license for the wrapper. |
| [`THIRD_PARTY_NOTICES`](../THIRD_PARTY_NOTICES) | Attribution and Boost Software License text for the ReactivePlusPlus dependency. |
| [`.clang-format`](../.clang-format) | Shared C++ formatting rules used locally and by CI. |
| [`.gitignore`](../.gitignore) | Excludes generated build files, binaries, logs, dependency installations and local IDE state from version control. |
| [`.gitmodules`](../.gitmodules) | Registers the Doxygen Awesome CSS theme as a Git submodule. |
| [`commitlint.config.mjs`](../commitlint.config.mjs) | Enforces Conventional Commit syntax, allowed types, lowercase scopes and the commit subject length limit. |

## Public headers and inline implementations

[`include/rppw/`](../include/rppw/) contains the project's public C++ headers
and their `.inl` implementation companions. This directory is installed for
consumers, and the public API uses the `rppw` namespace.

The reactive-type headers declare the four source categories: `Observable<T>`
for sequences, `Single<T>` for a required result, `Maybe<T>` for an optional
result, and `Completable` for completion without a value. Their corresponding
`.inl` files supply template and inline definitions and are included by the
headers; consumers do not need to include them directly.

Three headers have distinct supporting roles:

- [`ReactivePlusPlusWrapper.h`](../include/rppw/ReactivePlusPlusWrapper.h)
  is the umbrella include for all four reactive types, public concepts and the
  cancellation handle.
- [`Concepts.h`](../include/rppw/Concepts.h) defines reusable C++20 constraints
  for factories, callbacks, transformations, predicates and source ranges.
- [`Disposable.h`](../include/rppw/Disposable.h) declares the shared
  subscription-cancellation handle.

[`include/rppw/detail/`](../include/rppw/detail/) holds internal implementation
support. It is shipped with the headers but is not a public entry point:

- [`Fwd.h`](../include/rppw/detail/Fwd.h) provides reactive-type forward
  declarations and the internal constraint on supported value types.
- [`Operators.inl`](../include/rppw/detail/Operators.inl) provides shared helpers
  for deferred source creation and single/optional-result cardinality checks.

## Compiled library sources

[`src/`](../src/) contains the C++ implementation files compiled into the static
library. Most reactive operations live in the headers and `.inl` files; these
sources supply the out-of-line definitions:

- [`Completable.cpp`](../src/Completable.cpp) implements construction from an
  observable, completion/error factories and repeat/retry operations.
- [`Disposable.cpp`](../src/Disposable.cpp) implements cancellation-handle
  construction, disposal and disposal-state queries.

## CMake package support

[`cmake/`](../cmake/) contains package configuration support.
[`ReactivePlusPlusWrapperConfig.cmake.in`](../cmake/ReactivePlusPlusWrapperConfig.cmake.in)
is the template for the installed CMake package configuration. It locates the
ReactivePlusPlus and threading dependencies, loads the exported library targets
and checks requested components. This enables downstream
`find_package(ReactivePlusPlusWrapper CONFIG REQUIRED)` calls.

## Tests

[`tests/`](../tests/) contains C++ tests grouped by reactive type or behavior,
including composition, fallbacks, cancellation, scheduling, side effects,
timing and type constraints. These related test files are intentionally not
listed individually.

[`tests/CMakeLists.txt`](../tests/CMakeLists.txt) builds and registers:

- The GoogleTest/GoogleMock unit-test executable.
- A separate scheduler integration-test executable.
- Generated compile checks that include each public header independently.
- A multi-translation-unit public API consumer.

[`SchedulerIntegrationTests.cpp`](../tests/SchedulerIntegrationTests.cpp) is
the dedicated source for the separate scheduler integration executable,
whereas the other test suites belong to the unit-test executable.

[`tests/support/`](../tests/support/) contains reusable C++ test-support
headers: mock notification sinks, exception matchers, subscription fixtures
and bounded run-loop dispatch helpers.

[`tests/consumer/`](../tests/consumer/) contains a small consumer split across
multiple translation units to check that the public API compiles, links and
works across source-file boundaries.
[`ConsumerApi.h`](../tests/consumer/ConsumerApi.h) declares the source factories
shared by these translation units; the C++ sources define those factories and
the executable's validation entry point.

## Examples

[`examples/`](../examples/) contains standalone C++ usage examples for the four
reactive types and scheduling. The reactive-type examples are grouped rather
than enumerated.
[`scheduling.cpp`](../examples/scheduling.cpp) demonstrates explicit producer
and consumer run loops, notification dispatch and cancellation without creating
a worker thread.

[`examples/CMakeLists.txt`](../examples/CMakeLists.txt) creates the example
executables when `RPPW_BUILD_EXAMPLES` is enabled and registers them with CTest
when testing is also enabled.

[`examples/consumer/`](../examples/consumer/) is an independent CMake project
for testing the installed package rather than the source-tree target.
Its [`CMakeLists.txt`](../examples/consumer/CMakeLists.txt) uses `find_package`,
links the exported target and registers a consumer smoke test. Its C++ sources
split a small reactive pipeline across translation units. CI configures and
builds this project after installing the wrapper.

## Documentation and resources

[`docs/`](./) contains maintained Markdown documentation:

- [`conventional-commits.md`](conventional-commits.md) explains commit-message
  conventions and their CI enforcement.
- [`structure.md`](structure.md) is this project-layout guide.

[`resources/doxygen/`](../resources/doxygen/) contains API documentation inputs
and presentation resources:

- [`Doxyfile`](../resources/doxygen/Doxyfile) configures Doxygen input,
  validation and HTML generation under `build/doxygen`, including the theme.
- [`index.md`](../resources/doxygen/index.md) supplies the generated API
  documentation's main page, introducing the library and its usage.
- [`doxygen-awesome-css/`](../resources/doxygen/doxygen-awesome-css/) is the
  third-party Git submodule providing the documentation theme. Its upstream
  files are not part of the wrapper's own implementation.

## Automation

The YAML files in [`.github/workflows/`](../.github/workflows/) define GitHub
Actions automation, with separate validation and publication responsibilities:

- [`ci.yml`](../.github/workflows/ci.yml) validates commit messages, builds and
  tests Debug/Release/Profile configurations on Windows, Linux and macOS,
  checks an installed-package consumer, validates Doxygen documentation and
  checks C++ formatting.
- [`cd.yml`](../.github/workflows/cd.yml) generates Doxygen documentation after
  pushes to `master` and deploys the HTML output to the `gh-pages` branch.

## Generated and local directories

[`build/`](../build/) holds generated artifacts rather than maintained source:
CMake build trees, dependency installations, binaries, test results, logs,
installation staging directories, consumer builds and generated Doxygen HTML.
Its exact contents depend on the presets and commands run locally or in CI,
so individual generated files are not documented here.

Local `.git/` and `.vs/` directories hold Git metadata and Visual Studio state.
They are not part of the maintained source layout and are omitted from the
diagram.
