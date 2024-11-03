# CPP Utils

Simple C++ Utilities.

## Features

+ Console color
+ Cross-platform string encoding and print functions
+ Get command line arguments
+ Shared library loader
+ Simple plugin framework
+ Application information
+ String utilities and formater
+ PIMPL and VLA syntactic sugar

## Example

### Command Line

```cpp
#include <cpputils/system.h>

using namespace cpputils;

int main(int /* argc */, char * /* argv */[]) {
    // No need to use main entry arguments
    auto args = System::commandLineArguments();
    for (int i = 0; i < args.size(); ++i) {
        u8printf("%d - %s\n", i, args[i].data());
    }
    return 0;
}
```

```sh
> g++ -O2 main.cpp -o hello.exe
> hello.exe 123 Hello 你好 ◆ 😀 こんにちは
0 - test.exe
1 - 123
2 - Hello
3 - 你好
4 - ◆
5 - 😀
6 - こんにちは
```

## VLA

```cpp
#include <string_view>

#include <cpputils/vla.h>

// I need the command line in `string_view` array
extern void foo(const std::string_view cmdline[], int count);

int main(int argc, char *argv[]) {
    // No need to allocate memory on heap
    // Allocating on stack is a lot more effecient
    VLA_NEW(std::string_view, stack_argv, argc);
    for (int i = 0; i < argc; ++i) {
        stack_argv[i] = std::string_view(argv[i]);
    }
    process_command_lines(stack_argv, argc);
    return 0;
}
```