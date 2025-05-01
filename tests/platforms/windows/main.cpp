#include <iostream>

#include <stdcorelib/console.h>

#include <stdcorelib/platform/windows/registry.h>
#include <stdcorelib/platform/windows/winextra.h>

static int test_registry() {
    using namespace stdc::windows;

    RegKey rootKey(HKEY_CURRENT_USER);
    RegKey isKey = rootKey.open(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
    if (!isKey.isValid()) {
        stdc::console::critical("failed to open registry key: %1", rootKey.errorCode().message());
        return rootKey.errorCode().value();
    }

    stdc::console::u8println("Registry keys:");
    for (const auto &subkey : isKey.enumKeys()) {
        if (isKey.errorCode().value() != ERROR_SUCCESS) {
            stdc::console::critical("failed to enumerate registry key: %1",
                                    isKey.errorCode().message());
            return -1;
        }
        stdc::console::success("- %1", subkey.name);
    }
    stdc::console::u8println();

    stdc::console::u8println("Registry values:");
    for (const auto &val : isKey.enumValues()) {
        stdc::console::success("- %1", val.name);
    }
    if (isKey.errorCode().value() != ERROR_SUCCESS) {
        stdc::console::critical("failed to enumerate registry value: %1",
                                isKey.errorCode().message());
        return -1;
    }
    return 0;
}

void vare() {
    using namespace stdc;

    std::map<std::string, std::string> vars{
        {"FOO", "A"    },
        {"BAR", "B"    },
        {"A_B", "Hello"}
    };
    std::string actual = str::varexp("${${FOO}_${BAR}} World!", vars);
    std::string expect = "Hello World!";
    std::cout << actual << std::endl;
    std::cout << expect << std::endl;
}

int main(int argc, char *argv[]) {
    test_registry();
    vare();
    return 0;
}