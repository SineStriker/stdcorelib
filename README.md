# STDCORELIB

A small C++17 support library, for the things the standard library leaves to the platform.

Most of it exists because the answer differs between Windows and the rest of the world: console color, UTF-8 that survives a Windows console, launching a child process, loading a shared object. The rest is a handful of containers and utilities that kept getting rewritten.

Header-only where it can be, compiled where it has to be. No dependencies beyond the standard library, and Boost.Test for the test suite alone.

## Requirements

- C++17
- CMake 3.16 or newer
- Windows, Linux or macOS. MSVC, clang-cl, GCC and Clang are all built and tested.

## Building

```bash
git clone https://github.com/stdware/stdcorelib.git
cd stdcorelib
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/path/to/install
cmake --build build --config Release
cmake --install build --config Release
```

## Integration with CMake Projects

```cmake
find_package(stdcorelib REQUIRED)
target_link_libraries(myapp PRIVATE stdcorelib::stdcorelib)
```

The installed package config carries a version file with `AnyNewerVersion` compatibility, so a version argument is a floor rather than an exact match.

Or as a subdirectory:

```cmake
add_subdirectory(stdcorelib)
target_link_libraries(myapp PRIVATE stdcorelib::stdcorelib)
```

## What is in it

| Topic | |
| --- | --- |
| Command line | Declaring what a program takes, and reading back what it was given |
| Processes and libraries | Starting a child process, loading a shared object |
| Text | Strings, formatting, the console, UTF conversion |
| Containers and views | `array_view`, `vlarray`, `linked_map`, `any` |
| Type identity | Naming a type without RTTI, and registries built on that |
| Logging | Named categories with per-level switches and filter rules |
| JSON and CBOR | One tree, both encodings |
| Platform and system | Program and machine information, the Windows registry |
| Utilities | Flags, scope guards, version numbers |

Each topic carries its own description and an example. Build the `stdcorelib_docs` target with
`-DSTDC_BUILD_DOCS=ON`, or read the headers, which is where that text lives.

## License

MIT. See [LICENSE](LICENSE).

Parts are derived from other projects and keep their attribution in the source:

- [CPython](https://github.com/python/cpython)
- [qtbase](https://github.com/qt/qtbase)
- [xmake](https://github.com/xmake-io/xmake)
- [Registry](https://github.com/m4x1m1l14n/Registry)

The generated documentation is styled with
[doxygen-awesome-css](https://github.com/jothepro/doxygen-awesome-css), MIT, fetched at
configure time by `cmake/doxygen.cmake` when `STDC_BUILD_DOCS` is on, pinned to a tag and
checked against a hash. It is a stylesheet: nothing in the library uses it.
