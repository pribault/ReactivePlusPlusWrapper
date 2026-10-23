# ReactivePlusPlusWrapper

**Compose streams, optional results and actions with an RxJava-inspired API
in C++20.**

ReactivePlusPlusWrapper adds fluent, `camelCase` method chains and explicit
reactive types to [ReactivePlusPlus](https://github.com/AlexInLog/ReactivePlusPlus).
Use it to express a sequence of transformations as one pipeline, with error
handling and cancellation built into subscriptions.

[Read the documentation and API reference](https://pribault.github.io/ReactivePlusPlusWrapper/).

## A first pipeline

Keep the even numbers, multiply them by ten, then print the results:

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

Building a pipeline describes the work; calling `subscribe` starts it.
This example runs synchronously and creates no thread.

All four reactive types also offer `subscribe()` without callbacks. It starts
processing, ignores values and normal completion, and logs unhandled errors to
`stderr` without stopping the program. The returned `Disposable` can cancel the
subscription; destroying it does not cancel processing. Use the callback overload
when errors require application-specific handling.

## Why use this wrapper?

- **Familiar composition:** chain transformations and conversions using an API
  inspired by RxJava, while keeping ReactivePlusPlus as the execution engine.
- **Types that express intent:** distinguish a sequence, a required result,
  an optional result and an action without a return value.
- **Explicit control:** cancel subscriptions through a shared handle and choose
  schedulers for subscription and notification delivery.
- **A tested integration path:** CI builds on Windows, Linux and macOS and
  verifies that an independent application can consume the installed package.

This is not an exhaustive implementation of RxJava. It is a C++20 static library,
not a header-only package; ReactivePlusPlus remains a required dependency.

## Choose your reactive type

| Type | Successful outcome | Typical use |
| --- | --- | --- |
| `Observable<T>` | Zero or more values, then completion | Filter or transform a sequence |
| `Single<T>` | Exactly one value | Compute a required result |
| `Maybe<T>` | One value or empty completion | Look up an optional result |
| `Completable` | Completion without a value | Run an action or sequence work |

Every type can also terminate with an error. For `Maybe`, success with a value
and empty completion are separate outcomes: only one callback is called.

Include the header for the type you use, or
[`rppw/ReactivePlusPlusWrapper.h`](include/rppw/ReactivePlusPlusWrapper.h)
for the complete public API.

## Custom producers

All four types provide `create(producer)`. The const-invocable producer runs
independently for each subscription and receives an
`rpp::dynamic_observer<T>` (`std::monostate` for `Completable`):

```cpp
auto source = rppw::Single<int>::create(
    [](rpp::dynamic_observer<int> observer)
    {
        observer.on_next(42);
    });
```

`Single` and `Maybe` terminate on the first value, including when converted to
`Observable`. `Maybe` also accepts `on_completed()` without a value;
`Single` producers must emit a value or an error instead.
`Observable` accepts multiple values followed by `on_completed()`.
`Completable` ignores values and waits for `on_completed()`.

Producer exceptions are forwarded to the error callback. Observers can be
retained for asynchronous work. Serialize notifications, pass non-null errors,
and stop after termination or disposal. Cancellation suppresses notifications
but does not interrupt work automatically; attach cleanup with
`observer.set_upstream()`. Captured resources must outlive the work.

## Quick start

The steps below build the library, examples and tests in **Debug**.
Run each command only if the previous one succeeds.

### 1. Install the prerequisites

- CMake 3.25 or later and C++20.
- Ninja, Git and vcpkg.
- Windows: MSVC with x64 C++ tools and a Windows SDK; use an **x64** developer
  terminal before running CMake commands.
- Linux: C++20-capable GCC or Clang, pthreads and vcpkg bootstrap prerequisites
  (`curl`, `zip`, `unzip`, `tar`, build tools).
- macOS: Xcode Command Line Tools with a C++20-capable Apple Clang, plus
  CMake and Ninja.

The [vcpkg manifest](vcpkg.json) pins ReactivePlusPlus 2.2.3 and, for tests,
GoogleTest/GoogleMock 1.17.0. Configuration downloads dependencies and requires
network access; running tests and examples does not.

### 2. Set up vcpkg

If vcpkg is already installed at the pinned revision below, set `VCPKG_ROOT`
to that checkout and skip the clone.

**Windows**

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

With localized MSVC, select the UTF-8 code page before the first
configuration **and** for subsequent builds:

```powershell
chcp 65001
```

This preserves Ninja's parsing of `/showIncludes` dependencies. If the build
was already configured under another code page, rerun
`cmake --fresh --preset Debug` (and likewise for the other presets).

**Linux and macOS**

The example below installs vcpkg under `$HOME/tools/vcpkg`:

```sh
mkdir -p "$HOME/tools"
git clone https://github.com/microsoft/vcpkg.git "$HOME/tools/vcpkg"
git -C "$HOME/tools/vcpkg" checkout 40dd49b0c9c7dbd79dc64cad5e82308eae04f67f
"$HOME/tools/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/tools/vcpkg"
```

Keep using the same terminal for the following steps so `VCPKG_ROOT` remains set.
Ninja must be available in `PATH`.

### 3. Build and run

Clone the project if needed, then enter its root directory:

```sh
git clone https://github.com/pribault/ReactivePlusPlusWrapper.git
cd ReactivePlusPlusWrapper
```

These commands work on all three platforms:

```sh
cmake --preset Debug
cmake --build --preset Debug --parallel 4
ctest --preset Debug
```

Run your first example:

| Platform | Command |
| --- | --- |
| Windows (PowerShell) | `.\build\Debug\examples\rppw_observable.exe` |
| Linux or macOS | `./build/Debug/examples/rppw_observable` |

Expected output:

```text
20
40
complete
```

<details>
<summary>Other build configurations and options</summary>

Presets use Ninja with separate directories under `build`. To use another
configuration, replace `Debug` in the configure, build and test commands:

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

</details>

## Install and consume

To use the wrapper in your own application, install a library-only build.
With the prerequisites and `VCPKG_ROOT` from the quick start, run from the
project root:

```sh
cmake --preset Release -B build/library-release -DBUILD_TESTING=OFF -DRPPW_BUILD_EXAMPLES=OFF -DVCPKG_MANIFEST_FEATURES=
cmake --build build/library-release
cmake --install build/library-release --prefix build/install
```

This configuration does not require GoogleTest. In your application's
`CMakeLists.txt`, after defining its executable target:

```cmake
find_package(ReactivePlusPlusWrapper CONFIG REQUIRED)
target_link_libraries(my_application
    PRIVATE ReactivePlusPlusWrapper::ReactivePlusPlusWrapper)
```

The exported target propagates C++20, include paths, RPP and threading requirements.
The static archive, `.h` and `.inl` files are installed. RPP must remain available
to the consumer: the package calls `find_dependency(RPP CONFIG)`.

When configuring your application, add the wrapper's installation prefix and
the directory containing its installed dependencies to `CMAKE_PREFIX_PATH`.
The [independent consumer](examples/consumer/CMakeLists.txt) is a complete,
two-source-file example. On Windows:

```powershell
cmake -S examples\consumer -B build\consumer -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$PWD\build\install;$PWD\build\library-release\vcpkg_installed\x64-windows"
cmake --build build\consumer
ctest --test-dir build\consumer --output-on-failure
```

On Linux and macOS, use the same CMake options with your shell's line-continuation
syntax and paths. Replace `x64-windows` with the triplet used by your build
(for example, `x64-linux` or `arm64-osx`).
The wrapper package is relocatable; dependencies are not bundled.

## Examples and documentation

Each example uses only the public library target:

| Example | What it demonstrates | Executable |
| --- | --- | --- |
| [Observable](examples/observable.cpp) | Filtering and transforming a sequence | `rppw_observable` |
| [Single](examples/single.cpp) | A required result and an error path | `rppw_single` |
| [Maybe](examples/maybe.cpp) | An optional result and a fallback | `rppw_maybe` |
| [Completable](examples/completable.cpp) | Sequencing an action and a result | `rppw_completable` |
| [Scheduling](examples/scheduling.cpp) | Deferred subscription, notification delivery and cancellation | `rppw_scheduling` |

Executables are under `build/Debug/examples` after the quick start, with an
`.exe` suffix on Windows. Run all five with `ctest --preset Debug -L examples`.
The scheduling example pumps two run loops on the same thread; it does not
create a worker thread.

- [Online documentation](https://pribault.github.io/ReactivePlusPlusWrapper/):
  reactive concepts, usage and the API reference with individual method contracts.
- [Project structure](docs/structure.md): where to find headers, implementations,
  tests, examples and build support.
- [Public headers](include/rppw/): API declarations and Doxygen comments
  describing individual methods.

The documentation is available online without installing Doxygen.
To generate and check it locally, see [Contributing](#contributing).

## Contributing

Start with the [project structure guide](docs/structure.md) and run the Debug
build and tests from the quick start.

All repository-authored text must be in English, including documentation,
source comments, Doxygen, examples and commit messages (subject, body and footer prose).
Follow the [Conventional Commits guide](docs/conventional-commits.md).
CI validates commit syntax; English wording is a contribution and review requirement.

### Continuous integration

The [CI workflow](.github/workflows/ci.yml) runs on pushes and pull requests
targeting `develop` or `master`. It checks:

- Commit messages against the repository's Conventional Commits rules.
- Builds, tests, examples and standalone public headers on Windows 2022/MSVC,
  Ubuntu 24.04/GCC and macOS 14/Apple Clang, in Debug, Release and Profile.
- Installation and use by an independent consumer project.
- Doxygen documentation, with warnings treated as errors.
- C++ formatting using [.clang-format](.clang-format) and clang-format 21.1.8.

### Documentation and formatting checks

Install Doxygen and clang-format **21.1.8**, then run from the project root
in PowerShell:

```powershell
git submodule update --init
New-Item -ItemType Directory -Force build\doxygen | Out-Null
doxygen resources\doxygen\Doxyfile
$files = Get-ChildItem include,src,tests,examples -Recurse -File |
    Where-Object { $_.Extension -in ".h", ".hpp", ".inl", ".inc", ".cpp" }
clang-format --dry-run --Werror --style=file $files.FullName
```

Open `build/doxygen/html/index.html` for the generated API reference.
The [published documentation](https://pribault.github.io/ReactivePlusPlusWrapper/)
is available for browsing without a local build.
To apply formatting locally, replace `--dry-run --Werror` with `-i`.

## License

The wrapper is distributed under the [MIT license](LICENSE). ReactivePlusPlus is a
third-party dependency under the Boost Software License 1.0; its
[notice](THIRD_PARTY_NOTICES) is installed with the package.
