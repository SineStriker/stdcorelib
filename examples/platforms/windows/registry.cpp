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
    std::error_code ec;

    // HKCU is not owned by RegKey, no close will happen
    RegKey hkcuKey(RegKey::RK_CurrentUser);
    // the opened/created key is owned by RegKey, it will be closed when the RegKey destructs
    RegKey isKey =
        hkcuKey.open(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", ec);
    if (!isKey.isValid()) {
        console::critical("failed to open registry key: %1", ec.message());
        return -1;
    }

    // enumerate the subkeys
    console::u8println("Registry keys:");
    for (const auto &subkey : isKey.enumKeys(ec)) {
        console::success("- %1", subkey.name);
    }
    // must check for error after traversing, any error will cause the iteration to stop
    if (ec.value() != ERROR_SUCCESS) {
        console::critical("failed to enumerate registry keys: %1", ec.message());
        return -1;
    }
    console::u8println("Registry keys (for i=n-1; i>=0; i-=2):");
    {
        auto keys = isKey.enumKeys(ec);
        for (auto it = keys.rbegin(); it < keys.rend(); it += 2) {
            auto &subkey = *it;
            console::success("- %1", subkey.name);
        }
        if (ec.value() != ERROR_SUCCESS) {
            console::critical("failed to enumerate registry keys: %1", ec.message());
            return -1;
        }
    }

    // enumerate the value names
    console::u8println("Registry values:");
    for (const auto &val : isKey.enumValues(ec)) {
        console::success("- %1", val.name);
    }
    // same as above
    if (ec.value() != ERROR_SUCCESS) {
        console::critical("failed to enumerate registry values: %1", ec.message());
        return -1;
    }
    console::u8println("Registry values (for i=n-1; i>=0; i-=2):");
    {
        auto values = isKey.enumValues(ec);
        for (auto it = values.rbegin(); it < values.rend(); it += 2) {
            auto &val = *it;
            console::success("- %1", val.name);
        }
        if (ec.value() != ERROR_SUCCESS) {
            console::critical("failed to enumerate registry values: %1", ec.message());
            return -1;
        }
    }

    return 0;
}

static int example_RegistryQuery() {
    std::error_code ec;
    RegKey hklmKey(RegKey::RK_LocalMachine);
    RegKey systemKey = hklmKey.open(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", ec);
    if (!systemKey.isValid()) {
        console::critical("failed to open registry key: %1", ec.message());
        return -1;
    }

    RegValue productNameValue = systemKey.value(L"ProductName", ec);
    if (!productNameValue.isValid()) {
        console::critical("failed to query registry value: %1", ec.message());
        return -1;
    }

    console::u8println("Product Name:");
    console::success("  %1", productNameValue.toString());


    RegValue digitalProductIdValue = systemKey.value(L"DigitalProductId", ec);
    if (!digitalProductIdValue.isValid()) {
        console::critical("failed to query registry value: %1", ec.message());
        return -1;
    }

    console::u8println("Digital Product ID:");
    for (const auto &byte : digitalProductIdValue.toBinary()) {
        printf("%02X ", byte);
    }
    printf("\n");
    return 0;
}

int main(int argc, char *argv[]) {
    example_RegistryTraverse();
    example_RegistryQuery();
    return 0;
}