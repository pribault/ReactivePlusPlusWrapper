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

`Completable::toObservable<T>()` and `Completable::toMaybe<T>()` expose the
same deferred operation without emitting any value. Normal completion becomes
an empty Observable or Maybe; errors and cancellation are preserved.
The value type defaults to `std::monostate` and need not be default-constructible.

`Observable<T>` supports prefix selection with predicates: `takeWhile` excludes
the first false value; `takeUntil` includes the first true value and then
completes. Both cancel upstream immediately at that boundary.
`drop(count)` discards an initial count. `dropWhile` discards the initial true
prefix; `dropUntil` discards the initial false prefix. Both predicate-based
drop operators forward the boundary value and all later values without
evaluating the predicate again. State resets for each subscription, and
predicate exceptions use the reactive error channel.

`Observable<T>::allOf(predicate)`, `anyOf(predicate)` and `noneOf(predicate)`
return `Single<bool>` and cancel upstream as soon as the result is known.
Otherwise they wait for normal completion. On an empty source, `allOf` and
`noneOf` return true, and `anyOf` returns false. Evaluation is deferred for each
subscription; errors before a decisive result are propagated, not converted
to boolean results.

`Observable<T>::elementAt(index)`, `first()` and `last()` return `Maybe<T>`.
Indices are zero-based; a missing element produces an empty Maybe.
`elementAt` and `first` cancel upstream as soon as the selected value arrives.
`last` retains one value and waits for normal completion; an upstream error
is propagated instead of returning the retained value. All three methods are
lazy and execute independently for each subscription.

`Observable<T>::count()` returns `Single<std::size_t>` (zero on an empty source).
`min()` and `max()` return `Maybe<T>` (empty on an empty source), using
`std::less<>` by default or a supplied const-invocable strict weak ordering.
Equivalent extrema retain the first occurrence. These aggregations use
constant storage, wait for normal completion, propagate errors instead of
partial results, and reset for each subscription. Counter overflow signals
`std::overflow_error` through the reactive error channel.

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

### Combining source lists

`Completable::concat(sources)` and `Completable::merge(sources)` combine a
homogeneous range of `Completable` into one `Completable`.
`Observable<T>::concat(sources)` and `Observable<T>::merge(sources)` accept
homogeneous ranges of `Observable<T>`, `Single<T>` or `Maybe<T>` and return
an `Observable<T>`. Vectors, lists, arrays and input-range views are supported.

```cpp
const std::vector sources{rppw::Single<int>::just(1), rppw::Single<int>::just(2)};
const auto sequential = rppw::Observable<int>::concat(sources);
const auto merged = rppw::Observable<int>::merge(sources);
```

The range is copied immediately; source execution remains deferred and restarts
for each subscription. `concat` subscribes in order, waiting for each completion.
`merge` subscribes without waiting, preserving each source's value order but not
ordering between sources. It adds neither threads nor a concurrency limit;
synchronous sources still execute on the subscribing thread unless scheduled.
Empty ranges complete immediately, and empty Maybes emit nothing.
The first error terminates the result and cancels active sources. Disposing the
result cancels active subscriptions; `concat` never starts remaining sources after
an error or cancellation. Range traversal and allocation failures throw synchronously.

### Side-effect hooks

All four source types provide `doOnError(action)` and `doOnComplete(action)`.
`Single<T>` and `Maybe<T>` also provide `doOnSuccess(action)`;
`Observable<T>` provides `doOnNext(action)`.

Hooks run before the corresponding downstream notification, preserve the source
type and notifications, and do not modify the original source. Callbacks are
stored by value and run only during subscription, once per matching event.
Error callbacks receive `const std::exception_ptr&`; value callbacks receive
`const T&`. Completion callbacks take no arguments.

`doOnComplete` runs before any normal termination: before success for `Single`,
before success or empty completion for `Maybe`, and before completion for
`Completable` and `Observable`. It does not add a completion signal to `Single`
or to a successful `Maybe`. `doOnSuccess` is skipped for an empty `Maybe`.
Error and cancellation do not trigger completion hooks.

Like `subscribe` callbacks, hooks **must not throw**: an escaping exception calls
`std::terminate`. Hooks run in the upstream notification context; they add no
scheduler. Reference captures must outlive processing, and callers must
synchronize state shared across subscriptions.

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
- **Doxygen**: public API validation using
  [resources/doxygen/Doxyfile](resources/doxygen/Doxyfile); every
  documentation warning is an error. HTML output goes under `build/doxygen`.
- **clang-format 21.1.8**: read-only validation of C++ files in
  `include`, `src`, `tests` and `examples`, using [.clang-format](.clang-format).

The three quality checks run on Ubuntu, alongside the build matrix.
The vcpkg checkout is pinned to the same commit as the manifest.

To reproduce quality checks from the root:

```powershell
git submodule update --init
New-Item -ItemType Directory -Force build\doxygen | Out-Null
doxygen resources\doxygen\Doxyfile
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
`FlatMapperFor`, `RangeOf`, `ObservableSourceRangeOf`, `ReactiveSource` and `ExceptionValue`. Signatures use these
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
