#include <algorithm>

#include <stdcorelib/platform/windows/registry.h>
#include <stdcorelib/console.h>
#include <stdcorelib/scope_guard.h>

#include <boost/test/unit_test.hpp>

using namespace stdc::windows;

BOOST_AUTO_TEST_SUITE(test_registry)

BOOST_AUTO_TEST_CASE(test_reg_read_write) {
    std::error_code ec;

    std::wstring TEST_KEY = L"SOFTWARE\\test_registry";

    RegKey hkcuKey(RegKey::RK_CurrentUser);
    RegKey testKey =
        hkcuKey.create(TEST_KEY, ec, RegKey::DA_Read | RegKey::DA_Write, RegKey::CO_Volatile);
    BOOST_VERIFY(testKey.isValid());

    std::pair<std::wstring, RegValue> TEST_STRING =
        std::make_pair(L"test_string", RegValue(L"test_value"));
    std::pair<std::wstring, RegValue> TEST_STRING_NULL =
        std::make_pair(L"test_string_null", RegValue(RegValue::String));
    std::pair<std::wstring, RegValue> TEST_STRING_LIST = //
        std::make_pair(L"test_string_list",
                       RegValue({L"test_value1", L"test_value2", L"test_value3"}));
    std::pair<std::wstring, RegValue> TEST_STRING_LIST_NULL =
        std::make_pair(L"test_string_list_null", RegValue(RegValue::StringList));
    std::pair<std::wstring, RegValue> TEST_DWORD =
        std::make_pair(L"test_dword", RegValue(static_cast<uint32_t>(1234)));
    std::pair<std::wstring, RegValue> TEST_QWORD =
        std::make_pair(L"test_qword", RegValue(static_cast<uint64_t>(123456789)));
    std::pair<std::wstring, RegValue> TEST_BINARY =
        std::make_pair(L"test_binary", RegValue(std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04}));
    std::pair<std::wstring, RegValue> TEST_BINARY_NULL =
        std::make_pair(L"test_binary_null", RegValue(RegValue::Binary));
    std::pair<std::wstring, RegValue> TEST_NOT_EXIST = //
        std::make_pair(L"test_not_exist", RegValue());
    std::pair<std::wstring, RegValue> TEST_DEFAULT =
        std::make_pair(L"", RegValue(L"default_value"));

    auto deleteGuard = stdc::make_scope_guard([&]() {
        testKey.close();
        hkcuKey.removeKey(TEST_KEY); //
    });

    // Set values
    BOOST_VERIFY(testKey.setValue(TEST_STRING.first, TEST_STRING.second, ec));
    BOOST_VERIFY(testKey.setValue(TEST_STRING_NULL.first, TEST_STRING_NULL.second, ec));
    BOOST_VERIFY(testKey.setValue(TEST_STRING_LIST.first, TEST_STRING_LIST.second, ec));
    BOOST_VERIFY(testKey.setValue(TEST_STRING_LIST_NULL.first, TEST_STRING_LIST_NULL.second, ec));
    BOOST_VERIFY(testKey.setValue(TEST_DWORD.first, TEST_DWORD.second, ec));
    BOOST_VERIFY(testKey.setValue(TEST_QWORD.first, TEST_QWORD.second, ec));
    BOOST_VERIFY(testKey.setValue(TEST_BINARY.first, TEST_BINARY.second, ec));
    BOOST_VERIFY(testKey.setValue(TEST_BINARY_NULL.first, TEST_BINARY_NULL.second, ec));
    BOOST_VERIFY(testKey.setValue(TEST_DEFAULT.first, TEST_DEFAULT.second, ec));

    // Get values
    {
        RegValue val = testKey.value(TEST_STRING.first, ec);
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(val == TEST_STRING.second);
    }
    {
        RegValue val = testKey.value(TEST_STRING_NULL.first, ec);
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(val == TEST_STRING_NULL.second);
    }
    {
        RegValue val = testKey.value(TEST_STRING_LIST.first, ec);
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(val == TEST_STRING_LIST.second);
    }
    {
        RegValue val = testKey.value(TEST_STRING_LIST_NULL.first, ec);
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(val == TEST_STRING_LIST_NULL.second);
    }
    {
        RegValue val = testKey.value(TEST_DWORD.first, ec);
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(val == TEST_DWORD.second);
    }
    {
        RegValue val = testKey.value(TEST_QWORD.first, ec);
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(val == TEST_QWORD.second);
    }
    {
        RegValue val = testKey.value(TEST_BINARY.first, ec);
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(val == TEST_BINARY.second);
    }
    {
        RegValue val = testKey.value(TEST_BINARY_NULL.first, ec);
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(val == TEST_BINARY_NULL.second);
    }
    {
        RegValue val = testKey.value(TEST_NOT_EXIST.first, ec);
        BOOST_CHECK(ec.value() == ERROR_FILE_NOT_FOUND);
    }
    {
        RegValue val = testKey.value(TEST_DEFAULT.first, ec);
        BOOST_CHECK(ec.value() == ERROR_SUCCESS);
        BOOST_CHECK(val == TEST_DEFAULT.second);
    }

    // Remove values
    BOOST_VERIFY(testKey.removeValue(TEST_STRING.first, ec));
    BOOST_VERIFY(testKey.removeValue(TEST_STRING_NULL.first, ec));
    BOOST_VERIFY(testKey.removeValue(TEST_STRING_LIST.first, ec));
    BOOST_VERIFY(testKey.removeValue(TEST_STRING_LIST_NULL.first, ec));
    BOOST_VERIFY(testKey.removeValue(TEST_DWORD.first, ec));
    BOOST_VERIFY(testKey.removeValue(TEST_QWORD.first, ec));
    BOOST_VERIFY(testKey.removeValue(TEST_BINARY.first, ec));
    BOOST_VERIFY(testKey.removeValue(TEST_BINARY_NULL.first, ec));
    BOOST_VERIFY(testKey.removeValue(TEST_DEFAULT.first, ec));
}

BOOST_AUTO_TEST_CASE(test_regvalue) {

    std::wstring TEST_STRING_VALUE = L"test_string_value";
    std::vector<std::wstring> TEST_STRING_LIST_VALUE = {
        L"test_string_list_value1",
        L"test_string_list_value2",
        L"test_string_list_value3",
    };
    uint32_t TEST_DWORD_VALUE = 1234;
    uint64_t TEST_QWORD_VALUE = 123456789;
    std::vector<uint8_t> TEST_BINARY_VALUE = {0x01, 0x02, 0x03, 0x04};

    {
        RegValue val1(TEST_STRING_VALUE);
        RegValue val2(TEST_STRING_LIST_VALUE);
        RegValue val3(TEST_DWORD_VALUE);
        RegValue val4(TEST_QWORD_VALUE);
        RegValue val5(TEST_BINARY_VALUE);

        BOOST_CHECK(val1.type() == RegValue::String);
        BOOST_CHECK(val2.type() == RegValue::StringList);
        BOOST_CHECK(val3.type() == RegValue::Int32);
        BOOST_CHECK(val4.type() == RegValue::Int64);
        BOOST_CHECK(val5.type() == RegValue::Binary);

        BOOST_CHECK(val1.toString() == TEST_STRING_VALUE);
        BOOST_CHECK(val2.toStringList().vec() == TEST_STRING_LIST_VALUE);
        BOOST_CHECK(val3.toUInt32() == TEST_DWORD_VALUE);
        BOOST_CHECK(val4.toUInt64() == TEST_QWORD_VALUE);
        BOOST_CHECK(val5.toBinary().vec() == TEST_BINARY_VALUE);
    }

    const wchar_t TEST_STRING_LIST_LITERAL_1[] =
        L"test_string_list_value1\0test_string_list_value2\0test_string_list_value3";
    const wchar_t TEST_STRING_LIST_LITERAL_2[] =
        L"test_string_list_value1\0test_string_list_value2\0test_string_list_value3\0";
    const wchar_t TEST_STRING_LIST_LITERAL_3[] =
        L"test_string_list_value1\0test_string_list_value2\0test_string_list_value3\0\0";

    {
        RegValue val1(TEST_STRING_LIST_LITERAL_1, sizeof(TEST_STRING_LIST_LITERAL_1) / 2 - 1,
                      RegValue::StringList);
        RegValue val2(TEST_STRING_LIST_LITERAL_2, sizeof(TEST_STRING_LIST_LITERAL_2) / 2 - 1,
                      RegValue::StringList);
        RegValue val3(TEST_STRING_LIST_LITERAL_3, sizeof(TEST_STRING_LIST_LITERAL_3) / 2 - 1,
                      RegValue::StringList);

        BOOST_CHECK(val1.type() == RegValue::StringList);
        BOOST_CHECK(val2.type() == RegValue::StringList);
        BOOST_CHECK(val3.type() == RegValue::StringList);

        BOOST_CHECK(val1.toStringList().vec() == TEST_STRING_LIST_VALUE);
        BOOST_CHECK(val2.toStringList().vec() == TEST_STRING_LIST_VALUE);
        BOOST_CHECK(val3.toStringList().vec() == TEST_STRING_LIST_VALUE);
    }
}

BOOST_AUTO_TEST_CASE(test_regkey) {
    std::error_code ec;

    RegKey hklmKey(RegKey::RK_LocalMachine);
    RegKey systemKey = hklmKey.open(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", ec);
    BOOST_VERIFY(systemKey.isValid());

    {
        // test collect
        std::vector<std::wstring> subkeys;
        for (const auto &subkey : systemKey.enumKeys(ec)) {
            subkeys.push_back(subkey.name);
        }
        BOOST_VERIFY(ec.value() == ERROR_SUCCESS);

        // "Windows" should be one of the subkeys
        BOOST_CHECK(std::find(subkeys.begin(), subkeys.end(), L"Windows") != subkeys.end());

        // test enumerate
        {
            std::vector<std::wstring> reversedSubkeys;
            auto keys = systemKey.enumKeys(ec);
            for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
                reversedSubkeys.push_back(it->name);
            }
            BOOST_VERIFY(ec.value() == ERROR_SUCCESS);
            std::reverse(reversedSubkeys.begin(), reversedSubkeys.end());
            BOOST_CHECK(reversedSubkeys == subkeys);
        }
    }

    {
        // test collect
        std::vector<std::wstring> values;
        for (const auto &val : systemKey.enumValues(ec)) {
            values.push_back(val.name);
        }
        BOOST_VERIFY(ec.value() == ERROR_SUCCESS);

        // "ProductName" should be one of the values
        BOOST_CHECK(std::find(values.begin(), values.end(), L"ProductName") != values.end());

        // test enumerate
        {
            std::vector<std::wstring> reversedValues;
            auto vals = systemKey.enumValues(ec);
            for (auto it = vals.rbegin(); it != vals.rend(); ++it) {
                reversedValues.push_back(it->name);
            }
            BOOST_VERIFY(ec.value() == ERROR_SUCCESS);
            std::reverse(reversedValues.begin(), reversedValues.end());
            BOOST_CHECK(reversedValues == values);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()