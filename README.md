# STDCORELIB

C++ auxiliary core library.

## Features

### UTF-8 Encoding

- UTF-8 console output with colors and styles
- UTF-8 program information
- UTF-8 string encoding

### OS

- Shared library manipulator
- Python **Popen** re-implementation
- Windows Registry key/value manipulator

### String Extra
- Extra Apis (`starts_with`, `split`, `join`, `trim`...)
- Format string
- Variable expression (`${VAR}`)

## Build From Source

```bash
git clone https://github.com/SineStriker/stdcorelib.git
cd stdcorelib
cmake -B build -S. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/path/to/install
cmake --build build --config Release --target all
cmake --build build --config Release --target install
```

## TODO

- Popen Unix implementation, communicate...
- Registry implementation

## Credits

- [qtbase](https://github.com/qt/qtbase)
- [Python](https://github.com/python/cpython)
- [xmake](https://github.com/xmake-io/xmake)
- [llvm-project](https://github.com/llvm/llvm-project)
- [Registry](https://github.com/m4x1m1l14n/Registry)