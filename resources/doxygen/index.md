# ReactivePlusPlusWrapper

ReactivePlusPlusWrapper is a C++20 library that brings an RxJava-inspired API to
[ReactivePlusPlus](https://github.com/AlexInLog/ReactivePlusPlus).
It provides fluent, `camelCase` method chains for composing streams, single
results, optional results and actions without a return value.

The wrapper focuses on explicit reactive types, conversions and cancellable
subscriptions. It builds on ReactivePlusPlus rather than replacing its engine,
and it is not an exhaustive implementation of RxJava.

## Choose the right reactive type

| Type | Successful outcome | Typical use |
| --- | --- | --- |
| rppw::Observable | Zero or more values, followed by completion | Transforming and filtering a sequence |
| rppw::Single | Exactly one value | Computing a required result |
| rppw::Maybe | One value or empty completion | Looking up an optional result |
| rppw::Completable | Completion without a value | Running an action or sequencing work |

Each type can also terminate with an error. For `Maybe`, success with a value
and empty completion are separate terminal outcomes: `onSuccess` is not followed
by `onComplete`.

The public API lives in the `rppw` namespace. Include the header for the type you
need, or include `<rppw/ReactivePlusPlusWrapper.h>` for the complete public API.
Use the class reference for factories, operators, conversions and subscription
callback signatures.

## A first pipeline

Create a source, compose transformations, then subscribe to start processing:

```cpp
#include <exception>
#include <iostream>
#include <vector>

#include <rppw/Observable.h>

int main()
{
    bool failed = false;
    rppw::Observable<int>::fromIterable(std::vector{1, 2, 3, 4})
        .filter([](int value) { return value % 2 == 0; })
        .map([](int value) { return value * 10; })
        .subscribe(
            [](int value) { std::cout << value << '\n'; },
            [&failed](std::exception_ptr) {
                failed = true;
                std::cerr << "Observable failed\n";
            },
            [] { std::cout << "complete\n"; });
    return failed ? 1 : 0;
}
```

This synchronous example prints:

```text
20
40
complete
```

## Execution, errors and cancellation

- **Cold sources:** each subscription executes its own chain. Callable factories
  and actions run at subscription time, not when the pipeline is composed.
  `fromIterable` copies its input elements immediately and defers their emission.
- **Explicit error handling:** subscriptions require the callbacks appropriate
  to their reactive type, including an error callback. Producer and operator
  exceptions are delivered through `std::exception_ptr`.
- **Non-throwing callbacks:** callbacks passed to `subscribe` must not throw.
  An exception escaping a subscription callback terminates the program.
- **Shared cancellation:** `subscribe` returns an rppw::Disposable.
  Calling `dispose()` explicitly cancels the subscription and is idempotent.
  Copies share cancellation state; destroying a handle does not cancel.
- **Scheduler integration:** `subscribeOn` controls where subscription occurs;
  `observeOn` controls notification delivery. Without an explicit scheduler,
  synchronous sources run on the subscribing thread and create no thread.

Values must satisfy `std::copyable`, and factories, operators and subscription
callbacks must be invocable on a const callable object. Captured references must
outlive their use, including asynchronous processing.

There is no backpressure mechanism, `flatMap` concurrency limit or bounded
`observeOn` queue. Cancellation does not interrupt an action or callback already
in progress.

## Build and integrate

The project requires C++20, CMake 3.25 or later and ReactivePlusPlus.
The repository provides Ninja-based CMake presets and a vcpkg manifest for
dependency resolution. Development presets build the library, examples and
GoogleTest/GoogleMock tests.

After setting up the prerequisites described in the
[repository README](https://github.com/pribault/ReactivePlusPlusWrapper#readme),
run these commands from the repository root, stopping if a command fails:

```text
cmake --preset Debug
cmake --build --preset Debug --parallel 4
ctest --preset Debug
```

For an installed package, link the exported CMake target:

```cmake
find_package(ReactivePlusPlusWrapper CONFIG REQUIRED)
target_link_libraries(my_application
    PRIVATE ReactivePlusPlusWrapper::ReactivePlusPlusWrapper)
```

The target propagates the C++20 requirement, include paths, ReactivePlusPlus and
threading dependencies. The wrapper is a static library; ReactivePlusPlus must
remain available to the consuming project.

## Explore further

- [Source repository and setup guide](https://github.com/pribault/ReactivePlusPlusWrapper):
  prerequisites, installation instructions and contribution guidelines.
- [Runnable examples](https://github.com/pribault/ReactivePlusPlusWrapper/tree/master/examples):
  all four reactive types, scheduling and an independent CMake consumer.
- **API reference:** browse rppw::Observable, rppw::Single, rppw::Maybe,
  rppw::Completable and rppw::Disposable for detailed contracts.

## License

ReactivePlusPlusWrapper is distributed under the
[MIT license](https://github.com/pribault/ReactivePlusPlusWrapper/blob/master/LICENSE).
ReactivePlusPlus is a third-party dependency distributed under the Boost
Software License 1.0; see the
[third-party notices](https://github.com/pribault/ReactivePlusPlusWrapper/blob/master/THIRD_PARTY_NOTICES).
