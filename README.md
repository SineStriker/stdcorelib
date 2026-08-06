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
git clone https://github.com/SineStriker/stdcorelib.git
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

## Quick Start

### Console output

Text with attributes, and a UTF-8 story that holds up on a Windows console. Whether escapes are emitted at all is decided per target file, so redirecting to a file gets the text alone rather than a pile of escape sequences.

```cpp
#include <stdcorelib/console.h>
using namespace stdc;

console::printf(console::bold, console::lightgreen, console::nocolor, "%d passed\n", n);
console::warning("%1 is deprecated, use %2", old_name, new_name);
u8println("plain UTF-8, transcoded for the console if it needs it");
```

Attributes can travel inside the string instead of alongside it:

```cpp
cprintln("${lightgreen}ok ${@blue bold}on blue ${reset}plain, 50$$ off");
```

`console::set_color_mode()` is where a `--color=always` flag or `NO_COLOR` belongs.

### Strings

```cpp
#include <stdcorelib/str.h>
using namespace stdc;

auto msg  = formatN("%1 took %2 ms", name, elapsed);   // not printf, arguments carry their types
auto head = str::trim(str::split(line, ",").front());
auto path = str::join({"usr", "local", "bin"}, "/");

std::wstring w = wstring_conv::from_utf8(msg);         // and to_utf8, to_ansi, from_ansi
auto expanded  = str::varexp("${HOME}/config", env);   // ${VAR}, nested, $$ escapes
```

`formatN` takes anything `str::to_string` handles, which includes `std::filesystem::path` and wide strings, so there is nothing to convert at the call site.

### Paths and program information

```cpp
#include <stdcorelib/path.h>
#include <stdcorelib/system.h>
using namespace stdc;

auto dir  = system::application_directory();      // from the OS, not from argv[0]
auto args = system::command_line_arguments();     // UTF-8, from the wide command line on Windows
auto env  = system::environment();                // UTF-8 too, however the platform stores it
auto text = path::to_utf8(dir / "config.json");   // path::string() is the lossy one on Windows
auto tidy = path::clean_path(messy);              // resolves . and .. without touching the disk
```

### Child processes

A port of Python's `subprocess.Popen`, on both Windows and POSIX.

```cpp
#include <stdcorelib/support/popen.h>
using namespace stdc;

Popen proc;
proc.args({"git", "describe", "--tags"})
    .stdin_(Popen::DEVNULL)
    .stdout_(Popen::PIPE)
    .stderr_(Popen::STDOUT);

std::string err;
if (!proc.start(&err)) {
    return err;
}
auto [out, _] = proc.communicate({}, 5000);
int code = proc.returncode().value_or(-1);
```

The pipes are `std::iostream`, so the usual vocabulary works on them:

```cpp
std::string line;
while (std::getline(proc.stdout_(), line)) {
    use(line);
}
```

`communicate()` is the one to reach for when more than one pipe is open. Draining them by hand, one at a time, deadlocks as soon as the other one fills.

A `Popen` owns its child and kills it on the way out. `detached(true)` gives that up: the child is launched independently and this process keeps nothing but its pid.

### Logging

Named categories with per-level switches and Qt-style filter rules.

```cpp
#include <stdcorelib/support/logging.h>

static stdc::LogCategory lc("app.io");

lc.stdcWarning("cannot read %1", path);
lc.stdcDebugF("offset=%zu", off);       // printf-style variant
stdcInfo("no category in scope, so this goes to the default one");
```

```cpp
lc.setFilterRules("*.debug = false\n"      // silence debug everywhere
                  "app.io = false\n"       // silence this category
                  "app.io.warning = true"); // except for its warnings
```

`Logger::setLogCallback()` replaces the sink, which is how records reach a file or a UI instead of the terminal.

### Shared libraries

```cpp
#include <stdcorelib/support/sharedlibrary.h>
using namespace stdc;

SharedLibrary lib;
if (!lib.open(plugin_path)) {
    return lib.lastError();
}
auto entry = reinterpret_cast<int (*)()>(lib.resolve("plugin_init"));
```

`SharedLibrary::setLibraryPath()` is what lets a plugin outside the usual directories find the libraries next to it, and `locateLibraryPath()` answers where a given address came from.

### Windows registry

```cpp
#include <stdcorelib/platform/windows/registry.h>
using namespace stdc::windows;

std::error_code ec;
RegKey hklm(RegKey::RK_LocalMachine);
RegKey key = hklm.open(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", ec);
if (key.isValid()) {
    auto name = key.value(L"ProductName", ec).toString();
}

for (const auto &sub : key.enumKeys(ec)) {
    use(sub.name);
}
if (ec.value() != ERROR_SUCCESS) { // an error ends the loop quietly, so check after it
    return ec;
}
```

Every operation comes in two forms: one taking an `std::error_code` and `noexcept`, one without that throws.

### Containers and utilities

| Header | What it gives |
| --- | --- |
| `adt/array_view.h` | A read-only view over any contiguous container, so one parameter replaces a pile of overloads |
| `adt/vlarray.h` | A vector with inline storage for the first N elements, which stays off the heap while it is small |
| `adt/linked_map.h` | A map that remembers insertion order, over `std::unordered_map` or `std::map` |
| `stlextra/iterator.h` | A reverse iterator that stores the element it denotes, for iterators that carry state |
| `stlextra/algorithms.h` | `contains`, and a hash combiner that depends on the order of its arguments |
| `flags.h` | Type-safe bit flags over an enum, in the shape of `QFlags` |
| `scope_guard.h` | Run something on the way out, unless `dismiss()` says otherwise |
| `support/versionnumber.h` | A four-part version that parses, prints, compares and hashes |
| `vla.h` | `STDC_VLA_ALLOC` and `STDC_VLA_NEW`, stack arrays sized at run time |
| `pimpl.h` | The `stdc_impl_t` boilerplate used across the library |

```cpp
stdc::vlarray<int, 16> v;      // no allocation until the 17th element
auto guard = stdc::make_scope_guard([&] { std::fclose(f); });
auto ver = stdc::VersionNumber::fromString("1.2.3");
```

## License

MIT. See [LICENSE](LICENSE).

Parts are derived from other projects and keep their attribution in the source:

- [CPython](https://github.com/python/cpython)
- [qtbase](https://github.com/qt/qtbase)
- [xmake](https://github.com/xmake-io/xmake)
- [Registry](https://github.com/m4x1m1l14n/Registry)
