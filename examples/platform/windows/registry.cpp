#include <iostream>

#include <stdcorelib/console.h>

#include <stdcorelib/platform/windows/registry.h>
#include <stdcorelib/platform/windows/winextra.h>

using namespace stdc;

using namespace windows;

using console::success;
using console::critical;

/*!
    Enumerates the subkeys and values of a registry key.
*/
static int example_RegistryTraverse() {
    std::wstring subpath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
    u8println("Querying registry key: HKCU\\%1", subpath);

    std::error_code ec;

    // HKCU is not owned by RegKey, no close will happen
    RegKey hkcuKey(RegKey::RK_CurrentUser);
    // the opened/created key is owned by RegKey, it will be closed when the RegKey destructs
    RegKey isKey = hkcuKey.open(subpath, ec);
    if (!isKey.isValid()) {
        critical("failed to open registry key: %1", ec.message());
        return -1;
    }

    // enumerate the subkeys
    u8println("Keys:");
    for (const auto &subkey : isKey.enumKeys(ec)) {
        success("- %1", subkey.name);
    }
    // must check for error after traversing, any error will cause the iteration to stop
    if (ec.value() != ERROR_SUCCESS) {
        critical("failed to enumerate registry keys: %1", ec.message());
        return -1;
    }
    u8println("Keys (for i=n-1; i>=0; i-=2):");
    {
        auto keys = isKey.enumKeys(ec);
        for (auto it = keys.rbegin(); it < keys.rend(); it += 2) {
            auto &subkey = *it;
            success("- %1", subkey.name);
        }
        if (ec.value() != ERROR_SUCCESS) {
            critical("failed to enumerate registry keys: %1", ec.message());
            return -1;
        }
    }

    // enumerate the value names
    u8println("Value Names:");
    for (const auto &val : isKey.enumValues(ec)) {
        success("- %1", val.name);
    }
    // same as above
    if (ec.value() != ERROR_SUCCESS) {
        critical("failed to enumerate registry values: %1", ec.message());
        return -1;
    }
    u8println("Value Names (for i=n-1; i>=0; i-=2):");
    {
        auto values = isKey.enumValues(ec);
        for (auto it = values.rbegin(); it < values.rend(); it += 2) {
            auto &val = *it;
            success("- %1", val.name);
        }
        if (ec.value() != ERROR_SUCCESS) {
            critical("failed to enumerate registry values: %1", ec.message());
            return -1;
        }
    }
    u8println();
    return 0;
}

static int example_RegistryQuery() {
    std::wstring subpath = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    u8println("Querying registry key: HKCU\\%1", subpath);

    std::error_code ec;
    RegKey hklmKey(RegKey::RK_LocalMachine);
    RegKey systemKey = hklmKey.open(subpath, ec);
    if (!systemKey.isValid()) {
        critical("failed to open registry key: %1", ec.message());
        return -1;
    }

    RegValue productNameValue = systemKey.value(L"ProductName", ec);
    if (!productNameValue.isValid()) {
        critical("failed to query registry value: %1", ec.message());
        return -1;
    }

    u8println("Product Name:");
    success("%1", productNameValue.toString());

    RegValue digitalProductIdValue = systemKey.value(L"DigitalProductId", ec);
    if (!digitalProductIdValue.isValid()) {
        critical("failed to query registry value: %1", ec.message());
        return -1;
    }

    u8println("Digital Product ID:");
    {
        int i = 0;
        for (const auto &byte : digitalProductIdValue.toBinary()) {
            i++;
            cprintf("${lightgreen}%02X ", byte);
            if (i % 16 == 0)
                u8println();
        }
    }
    u8println();
    return 0;
}

int main(int argc, char *argv[]) {
    example_RegistryTraverse();
    example_RegistryQuery();
    return 0;
}