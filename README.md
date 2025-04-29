# STDCORELIB

C++ auxiliary core library.

## Features

+ Console color
+ Cross-platform string encoding
+ Program information
+ Shared library
+ String utilities and formatter
+ PIMPL and VLA syntactic sugar
+ Python **Popen** re-implementation (Experimental)

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

### Popen

```cpp
#include <fstream>

#include <stdcorelib/console.h>
#include <stdcorelib/support/popen.h>

int main(int argc, char *argv[]) {
    stdc::Popen proc;
    proc.args({"git", "--version"})
        .text(true)
        .stdout_(stdc::Popen::PIPE)
        .stderr_(stdc::Popen::STDOUT);
    std::string err_msg;
    if (!proc.start(&err_msg)) {
        stdc::u8println("Failed to start process: %1", err_msg);
        return -1;
    }

    FILE *out = proc.stdout_();
    std::filebuf buf(out);
    std::istream is(&buf);
    std::string line;
    while (std::getline(is, line)) {
        stdc::u8println(line);
    }
    stdc::u8println();
    proc.wait();

    int code = proc.returncode().value_or(-1);
    if (code == 0) {
        stdc::console::success("Process exit with code %1", code);
    } else {
        stdc::console::critical("Process exit with code %1", code);
    }
    return 0;
}
```

```sh
> g++ -O2 main.cpp -o test_git.exe
> test_git.exe
git version 2.47.1.windows.1

Process exit with code 0
```