# STDCORELIB

C++ auxiliary core library.

## Features

+ Console color
+ Cross-platform string encoding and printing functions
+ Program information
+ Shared library
+ String utilities and formatter
+ PIMPL and VLA syntactic sugar
+ Simple subprocess utility (Experimental)

## Example

### Command Line

```cpp
#include <stdcorelib/system.h>

int main(int /* argc */, char * /* argv */[]) {
    // No need to use main entry arguments
    auto args = stdc::system::command_line_arguments();
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