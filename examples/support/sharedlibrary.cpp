#include <stdcorelib/console.h>
#include <stdcorelib/support/sharedlibrary.h>

using namespace stdc;

/*!
    Loads a shared library and resolves a function symbol.
*/
int example_SharedLibrary() {
    SharedLibrary lib;

#ifdef _WIN32
    std::filesystem::path path = L"C:\\Windows\\System32\\kernel32.dll";
#else
    std::filesystem::path path = "/usr/lib/x86_64-linux-gnu/libc.so.6";
#endif

    // dlopen
    u8println("Loading library: %1", path);
    if (!lib.open(path)) {
        u8println("Failed to load library: %1", lib.lastError());
        return -1;
    }
    cprintf("${lightgreen}Handle: %p\n", lib.handle());

#ifdef _WIN32
    const char *funcName = "GetProcessHeap";
#else
    const char *funcName = "malloc";
#endif

    // dlsym
    u8println("Resolving function: %1", funcName);
    auto func = lib.resolve(funcName);
    if (!func) {
        u8println("Failed to resolve function: %1", lib.lastError());
        return -1;
    }
    cprintf("${lightgreen}Address: %p\n", func);

    return 0;
}


int main(int argc, char *argv[]) {
    example_SharedLibrary();
    return 0;
}