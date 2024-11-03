# CPP Utils

Simple C++ Utilities.

## Command Line

```c++
std::vector<std::string> commandLineArguments();

int main(int /* argc */, char * /* argv */[]) {
    auto args = commandLineArguments();
    for (int i = 0; i < args.size(); ++i) {
        u8printf("%-4d %s\n", i, args[i].data());
    }
    return 0;
}
```

```sh
> getcmdline.exe a 33 我 ◆ 😀
0    getcmdline.exe
1    a
2    33
3    我
4    ◆
5    😀
```

The command line arguments are returned as UTF-8 encoded `std::string` list on all platforms including Windows.