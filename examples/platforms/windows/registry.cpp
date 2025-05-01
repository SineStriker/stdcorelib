#include <iostream>

#include <stdcorelib/console.h>

#include <stdcorelib/platform/windows/registry.h>
#include <stdcorelib/platform/windows/winextra.h>

using namespace stdc;

using namespace windows;

/*!
    Enumerates the subkeys and values of a registry key.
*/
static int example_RegistryTraverse() {
    // HKCU is not owned by RegKey, no close will happen
    RegKey rootKey(RegKey::RK_CurrentUser);
    // the opened/created key is owned by RegKey, it will be closed when the RegKey destructs
    RegKey isKey = rootKey.open(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
    if (!isKey.isValid()) {
        console::critical("failed to open registry key: %1", rootKey.errorCode().message());
        return -1;
    }

    //
    // enumerate the subkeys
    //
    console::u8println("Registry keys:");
    for (const auto &subkey : isKey.enumKeys()) {
        console::success("- %1", subkey.name);
    }
    // must check for error after traversing, any error will cause the iteration to stop
    if (isKey.errorCode().value() != ERROR_SUCCESS) {
        console::critical("failed to enumerate registry keys: %1", isKey.errorCode().message());
        return -1;
    }

    //
    // enumerate the value names
    //
    console::u8println("Registry values:");
    for (const auto &val : isKey.enumValues()) {
        console::success("- %1", val.name);
    }
    // same as above
    if (isKey.errorCode().value() != ERROR_SUCCESS) {
        console::critical("failed to enumerate registry values: %1", isKey.errorCode().message());
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    example_RegistryTraverse();
    return 0;
}