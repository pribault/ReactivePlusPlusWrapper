# ReactivePlusPlusWrapper

C++20 library exposing an RxJava-inspired API on top of
[ReactivePlusPlus](https://github.com/AlexInLog/ReactivePlusPlus).

## API

| Class | Result |
| --- | --- |
| `Observable<T>` | Zero or more values, followed by completion or an error |
| `Single<T>` | One value or an error |
| `Maybe<T>` | One value, empty completion or an error |
| `Completable` | Completion without a value or an error |

The API provides `camelCase` method chains, explicit conversions,
cancellable subscriptions and ReactivePlusPlus scheduler integration.
It is not an exhaustive implementation of RxJava.

```cpp
// stl
#include <exception>
#include <iostream>
#include <vector>

// rppw
#include <rppw/Observable.h>

int main()
{
    bool failed = false;
    rppw::Observable<int>::fromIterable(std::vector{1, 2, 3, 4})
        .filter([](int value) { return value % 2 == 0; })
        .map([](int value) { return value * 10; })
        .subscribe(
            [](int value) { std::cout << value << '\n'; },
            [&failed](std::exception_ptr) { failed = true; std::cerr << "Observable failed\n"; },
            [] { std::cout << "complete\n"; });
    return failed ? 1 : 0;
}
```

Output: `20`, `40`, then `complete`, each on its own line.

## Prerequisites

- CMake 3.25 or later and C++20.
- Ninja, Git and vcpkg.
- Windows: MSVC with x64 C++ tools and a Windows SDK; use an **x64** developer
  terminal before running CMake commands.
- Linux: C++20-capable GCC or Clang, pthreads and vcpkg bootstrap prerequisites
  (`curl`, `zip`, `unzip`, `tar`, build tools).

The `vcpkg.json` manifest pins dependency resolution to commit
`40dd49b0c9c7dbd79dc64cad5e82308eae04f67f` : ReactivePlusPlus 2.2.3,
GoogleTest/GoogleMock 1.17.0. The old and new RPP repository URLs point to
the same 2.2.3 tag; no overlay is required.

### Set up vcpkg on Windows

PowerShell example, from the developer terminal:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\tools\vcpkg
git -C C:\tools\vcpkg checkout 40dd49b0c9c7dbd79dc64cad5e82308eae04f67f
& C:\tools\vcpkg\bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = "C:\tools\vcpkg"
```

Stop at the first error. If you initialize `vcvars64.bat` manually,
set `VCPKG_ROOT` **after** initialization: Visual Studio may replace it
with the path to its bundled vcpkg installation.

On Linux, clone the same commit, run `bootstrap-vcpkg.sh -disableMetrics`
and export `VCPKG_ROOT` pointing to that clone. Ninja must be available in `PATH`.
Downloading dependencies requires network access during configuration,
never while running tests or examples.

With localized MSVC, select the UTF-8 code page before the first
configuration **and** for subsequent builds:

```powershell
chcp 65001
```

This preserves Ninja's parsing of `/showIncludes` dependencies. If the build
was already configured under another code page, rerun
`cmake --fresh --preset Debug` (and likewise for the other presets).

## Build and test

From the project root:

```powershell
cmake --preset Debug
cmake --build --preset Debug --parallel 4
ctest --preset Debug

cmake --preset Release
cmake --build --preset Release --parallel 4
ctest --preset Release

cmake --preset Profile
cmake --build --preset Profile --parallel 4
ctest --preset Profile
```

Run each command only if the previous one succeeds. Presets use
Ninja with separate directories under `build`. CTest discovers GoogleTest cases;
discovering no tests is an error.

| Preset | Configuration | Tests | Examples |
| --- | --- | --- | --- |
| `Debug` | Debug | Yes | Yes |
| `Release` | Release | Yes | Yes |
| `Profile` | RelWithDebInfo (optimized with debug information) | Yes | Yes |

`BUILD_TESTING` and `RPPW_BUILD_EXAMPLES` control the corresponding targets.
The vcpkg `tests` feature, enabled by the development presets, supplies
GoogleTest/GoogleMock. For manual configuration with tests, also enable
`-DVCPKG_MANIFEST_FEATURES=tests`. No test framework is linked publicly.
Local settings belong in the untracked `CMakeUserPresets.json`.

## Continuous integration

The [GitHub Actions workflow](.github/workflows/ci.yml) runs on pushes and
pull requests targeting `develop` or `master`. Four independent jobs:

- **Commit messages**: commitlint validation using
  [the repository's Conventional Commits types and rules](docs/conventional-commits.md).
- **Build and tests**: Windows 2022/MSVC, Ubuntu 24.04/GCC and macOS 14/Apple
  Clang, each in Debug, Release and Profile using presets. Builds the library,
  tests, examples and standalone headers, then runs all CTest tests.
  The package is also installed and consumed by an independent project.
- **Doxygen**: public API validation using [Doxyfile](Doxyfile); every
  documentation warning is an error. HTML/XML output goes under `build/doxygen`.
- **clang-format 21.1.8**: read-only validation of C++ files in
  `include`, `src`, `tests` and `examples`, using [.clang-format](.clang-format).

The three quality checks run on Ubuntu, alongside the build matrix.
The vcpkg checkout is pinned to the same commit as the manifest.

To reproduce quality checks from the root:

```powershell
New-Item -ItemType Directory -Force build\doxygen | Out-Null
doxygen Doxyfile
$files = Get-ChildItem include,src,tests,examples -Recurse -File |
    Where-Object { $_.Extension -in ".h", ".hpp", ".inl", ".inc", ".cpp" }
clang-format --dry-run --Werror --style=file $files.FullName
```

Install Doxygen and clang-format **21.1.8** before running these commands.
To apply formatting locally, replace `--dry-run --Werror` with `-i`.

## Examples

Each example uses only the library's public target.

| Source | Executable | Output, one entry per line |
| --- | --- | --- |
| [Observable](examples/observable.cpp) | `rppw_observable` | `20`, `40`, `complete` |
| [Single](examples/single.cpp) | `rppw_single` | `42`, `expected failure` |
| [Maybe](examples/maybe.cpp) | `rppw_maybe` | `found`, `fallback` |
| [Completable](examples/completable.cpp) | `rppw_completable` | `prepare`, `42` |
| [Scheduling](examples/scheduling.cpp) | `rppw_scheduling` | `queued`, `42`, `cancelled` |

On Windows:

```powershell
.\build\Debug\examples\rppw_observable.exe
.\build\Debug\examples\rppw_scheduling.exe
```

On Linux, the corresponding executables have no `.exe` extension.
All five examples are also registered as CTest tests when testing
is enabled (`ctest --preset Debug -L examples`).

The scheduling example explicitly pumps two `rpp::schedulers::run_loop` instances
on the same thread. It demonstrates deferred subscription, deferred notifications
and cancellation with a bounded dispatch budget, leaving no background worker.
RPP schedulers can also target other threads.

## Install and consume

The library-only configuration does not resolve GoogleTest:

```powershell
cmake --preset Release -B build\library-release `
    -DBUILD_TESTING=OFF -DRPPW_BUILD_EXAMPLES=OFF -DVCPKG_MANIFEST_FEATURES=
cmake --build build\library-release
cmake --install build\library-release --prefix build\install
```

In a consumer project:

```cmake
find_package(ReactivePlusPlusWrapper CONFIG REQUIRED)
target_link_libraries(my_application
    PRIVATE ReactivePlusPlusWrapper::ReactivePlusPlusWrapper)
```

The exported target propagates C++20, include paths, RPP and threading requirements.
The static archive, `.h` and `.inl` files are installed. RPP must remain available
to the consumer: the package calls `find_dependency(RPP CONFIG)`.

The [independent consumer](examples/consumer/CMakeLists.txt), with two translation
units, verifies installation. Windows example:

```powershell
cmake -S examples\consumer -B build\consumer -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$PWD\build\install;$PWD\build\library-release\vcpkg_installed\x64-windows"
cmake --build build\consumer
ctest --test-dir build\consumer --output-on-failure
```

Adjust the triplet (`x64-linux` on Linux) and prefix as needed.
The wrapper package is relocatable; dependencies are not bundled.

## Important contracts

- Sources are cold. A callable is invoked only at subscription time,
  and again for each subscription. `fromIterable` immediately copies elements
  into an owned collection, then defers their emission.
- `subscribe` requires all callbacks for the type, including `onError`.
  Producer and operator exceptions are propagated as
  `std::exception_ptr`. Null errors are rejected during construction.
- All four types also provide `error(E)` for any copy-constructible
  error, even without deriving from `std::exception`:
  `Single<int>::error(std::runtime_error{"failure"})`. This overload calls
  `std::make_exception_ptr` immediately and retains the result for each
  subscription. Arguments convertible to `std::exception_ptr`, including `nullptr`,
  retain the existing overload and its validation.
- Callbacks passed to `subscribe` must not throw; a violation terminates
  the program. `Maybe` invokes either `onSuccess` or, when empty, `onComplete`,
  never both.
- `singleOrError` and `singleElement` retain the first value until
  completion and immediately reject a second value. An upstream error is
  never replaced with success.
- `Disposable::dispose()` explicitly and idempotently cancels.
  Copying the handle shares cancellation; destroying a handle does not cancel.
  A callback already in progress may finish after concurrent cancellation.
- Without an explicit scheduler, no thread is created. `subscribeOn` controls
  subscription; `observeOn` controls notification delivery.
- Values must satisfy `std::copyable`; reference captures must
  outlive asynchronous processing. RPP type erasure may allocate.
- No backpressure, `flatMap` concurrency limit or bounded queue
  for `observeOn`. User actions already in progress are not interrupted.

Factories, operator functions and subscription callbacks must be
invocable on a const callable object. Lambdas with `mutable` captures
are outside the contract.

Constraints are named in [Concepts.h](include/rppw/Concepts.h), under
`rppw::concepts`: `FactoryFor`, `SourceFactoryFor`, `CallbackFor`, `MapperFor`, `PredicateFor`,
`FlatMapperFor`, `RangeOf`, `ReactiveSource` and `ExceptionValue`. Signatures use these
concepts directly to reject incompatible calls during overload selection,
particularly for `map`, `flatMap` and `andThen`.
`map` requires an owned, copyable, non-reference result; `flatMap` requires
exactly the same reactive category as its source. Transformations
accept values by const reference or by value with the same
result type. Subscription callback results are ignored;
their constraint does not require a `noexcept` declaration.

The [detailed contracts and conversions](docs/technical-specification.md) are authoritative.

## Layout

Headers and templates are under `include\rppw`, non-template code under `src`,
GoogleTest/GoogleMock tests under `tests`, and demonstration programs under `examples`.
Each class is declared in its own public header, ending with
its `.inl` include. Shared forward declarations remain in
`detail/Fwd.h`; each public header can be included on its own.

Includes are grouped by origin, with a comment before each
group: `// stl`, `// rpp`, `// rppw`, `// googletest` or `// tests`.
The `.inl` includes at the end of headers follow the same convention.

## Documentation

- [Technical specification](docs/technical-specification.md): architecture,
  reactive contracts, API scope, dependencies and delivery criteria.
- [Implementation plan](docs/tasks.md): tasks, dependencies, tests and acceptance
  criteria.
- [Validation](docs/validation.md): environment, executed commands and limitations.

## Contributions

All repository-authored text must be in English, including documentation,
source comments, Doxygen, examples and commit messages (subject, body and footer prose).
Follow the [Conventional Commits guide](docs/conventional-commits.md).
CI validates commit syntax; English wording is a contribution and review requirement.

## License

The wrapper is distributed under the [MIT license](LICENSE). ReactivePlusPlus is a
third-party dependency under the Boost Software License 1.0; its
[notice](THIRD_PARTY_NOTICES) is installed with the package.
