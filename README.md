# prognot

A C++20 header-only library for exact, weighted progress reporting and
cooperative cancellation. Algorithms report progress independently of their UI.
Nested tasks share one operation, progress total, and cancellation source.

## Requirements and build

- C++20 compiler and standard library (including `std::stop_token`).
- CMake 3.28 or newer.
- Boost 1.85 or newer, with its CMake package installed; tested with Boost 1.90.
  Only headers are used: Rational and Multiprecision in the core, and
  Boost.Test (included runner) in unit tests. No compiled Boost.Test library is required.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /your/install/prefix
```

Use `-DBoost_DIR=/path/to/lib/cmake/Boost-1.xx.0` to select a Boost installation
when several versions are available. Dependencies are never downloaded implicitly.
Use `-DBUILD_TESTING=OFF` to build/install only the library.

Consumers can use `add_subdirectory()` or an installed package:

```cmake
find_package(prognot 0.2 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE prognot::prognot)
```

The target propagates C++20, Boost headers, and the platform thread dependency.

## Generated API reference

The public core interface is documented with Doxygen comments. With Doxygen
installed (verified with 1.15.0), generate the HTML reference from the repository root:

```sh
mkdir -p build
doxygen Doxyfile
```

Open `build/doxygen/html/index.html`. Documentation warnings are treated as errors.
The configuration covers the core only; deferred Qt interfaces are excluded.

## Simple callbacks

```cpp
auto progress = prognot::makeProgressNotifier([](const prognot::Progress& value) {
    // Report value; return false to request cancellation.
    return true;
});
progress.setStepCount(10);
progress.increment();
progress.finish();
```

The `ProgressCallback` concept checks the callable signature at compile time.
Use the virtual implementation interface below for lifecycle notifications.

## Weighted subtasks

```cpp
#include <prognot/prognot.hpp>
#include <iostream>
#include <memory>

struct Console : prognot::AProgressNotifierImpl {
    bool onIncrement(prognot::Progress value) override {
        std::cout << boost::rational_cast<double>(value) * 100 << "%\n";
        return true; // false requests cancellation
    }
};

int main() {
    auto output = std::make_shared<Console>();
    prognot::ProgressNotifier operation(output);
    operation.setStepCount(10);
    operation.increment(2); // 20%
    {
        auto loading = operation.subTaskNotifier(3); // reserves 30%
        loading.setStepCount(100);
        loading.increment(50); // total: 35%
        loading.finish();      // total: 50%
    }
    operation.finish();        // total: 100%
}
```

Call `finish()` explicitly when callback exceptions must be observable. Ordinary
scope exit completes a configured task's remaining unallocated steps as a fallback.
Unwinding and cancellation do not report synthetic completion.

See [the API contract](docs/api.md) for lifetime, cancellation, threading, iterator,
and migration rules. The historical Qt sources and examples are retained for a
later milestone; they are not built or installed by this core-only release.

No project license has been selected yet; this milestone does not assign one.
