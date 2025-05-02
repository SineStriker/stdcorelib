#include <algorithm>

#include <stdcorelib/platform/windows/registry.h>
#include <stdcorelib/console.h>

#include <boost/test/unit_test.hpp>

using namespace stdc::windows;

BOOST_AUTO_TEST_SUITE(test_registry)

BOOST_AUTO_TEST_CASE(test_regvalue) {
    // TODO
}

BOOST_AUTO_TEST_CASE(test_regkey) {
    RegKey hkcuKey(RegKey::RK_LocalMachine);
    RegKey systemKey = hkcuKey.open(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
    BOOST_CHECK(systemKey.isValid());

    // "Windows" should be one of the subkeys
    {
        std::error_code ec;
        std::vector<std::wstring> subkeys;
        for (const auto &subkey : systemKey.enumKeys(ec)) {
            subkeys.push_back(subkey.name);
        }
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(std::find(subkeys.begin(), subkeys.end(), L"Windows") != subkeys.end());
    }

    // "ProductName" should be one of the values
    {
        std::error_code ec;
        std::vector<std::wstring> values;
        for (const auto &val : systemKey.enumValues(ec)) {
            values.push_back(val.name);
        }
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(std::find(values.begin(), values.end(), L"ProductName") != values.end());
    }

    // TODO: add more tests
}

BOOST_AUTO_TEST_SUITE_END()