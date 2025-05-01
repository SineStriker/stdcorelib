#include <iostream>

#include <stdcorelib/console.h>

#include <stdcorelib/platform/windows/registry.h>
#include <stdcorelib/platform/windows/winextra.h>

/*!
    Enumerates the subkeys and values of a registry key.
*/
static int example_Registry() {
    using namespace stdc::windows;

    RegKey rootKey(RegKey::RK_CurrentUser);
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

int main(int argc, char *argv[]) {
    example_Registry();
    return 0;
}