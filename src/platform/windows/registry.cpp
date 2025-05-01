#include "registry.h"

#include <optional>
#include <variant>
#include <cassert>

#include "winapi.h"
#include "str.h"
#include "console.h"

namespace stdc::windows {

    struct RegValue::str {
        mutable std::variant<std::monostate, std::vector<uint8_t>, std::wstring> s;
        mutable std::optional<std::vector<std::wstring>> ms;

        void convertMultiStringToVector() const {
            assert(ms);

            std::wstring env_str;
            for (const auto &item : std::as_const(ms.value())) {
                env_str += item;
                env_str.push_back(L'\0');
            }
            env_str.push_back(L'\0');
            s = std::move(env_str);
        }

        void convertVectorToMultiString() const {
            assert(s.index() == 2);

            std::vector<std::wstring> ms;
            if (wchar_t *multiStrings = std::get<2>(s).data()) {
                for (const wchar_t *entry = multiStrings; *entry;) {
                    std::wstring entryStr(entry);
                    auto entryLen = entryStr.size();
                    ms.emplace_back(std::move(entryStr));
                    entry += entryLen + 1;
                }
            }
            ms = std::move(ms);
        }
    };

    /*!
        \class RegValue
        \brief A registry value holder.
    */

    RegValue::RegValue(Type type) : t(Invalid) {
    }

    RegValue::RegValue(const uint8_t *data, int size) : t(Binary), s(std::make_shared<str>()) {
        s->s = std::vector<uint8_t>(data, data + size);
    }

    RegValue::RegValue(std::vector<uint8_t> &&data, int size)
        : t(Binary), s(std::make_shared<str>()) {
        s->s = std::move(data);
    }

    RegValue::RegValue(int32_t value) : t(Int32) {
        d.dw = value;
    }

    RegValue::RegValue(int64_t value) : t(Int64) {
        d.qw = value;
    }

    RegValue::RegValue(const std::wstring &value, Type type) : t(type), s(std::make_shared<str>()) {
        s->s = value;
    }

    RegValue::RegValue(std::wstring &&value, Type type) : t(type), s(std::make_shared<str>()) {
        s->s = std::move(value);
    }

    RegValue::RegValue(const wchar_t *value, int size, Type type)
        : t(type), s(std::make_shared<str>()) {
        s->s = size < 0 ? std::wstring(value) : std::wstring(value, size);
    }

    RegValue::RegValue(const std::vector<std::wstring> &value)
        : t(MultiString), s(std::make_shared<str>()) {
        s->ms = value;
    }

    RegValue::RegValue(std::vector<std::wstring> &&value)
        : t(MultiString), s(std::make_shared<str>()) {
        s->ms = std::move(value);
    }

    RegValue::RegValue(const void *data, Type type) : t(type) {
        d.p = data;
    }

    RegValue::~RegValue() = default;

    RegValue::RegValue(const RegValue &RHS) = default;

    RegValue::RegValue(RegValue &&RHS) noexcept = default;

    RegValue &RegValue::operator=(const RegValue &RHS) = default;

    RegValue &RegValue::operator=(RegValue &&RHS) noexcept = default;

    const std::vector<uint8_t> &RegValue::toBinary() const {
        static std::vector<uint8_t> empty;
        if (!isBinary()) {
            return empty;
        }
        return std::get<1>(s->s);
    }

    int32_t RegValue::toInt32() const {
        if (!isInt32()) {
            return 0;
        }
        return d.dw;
    }

    int64_t RegValue::toInt64() const {
        if (!isInt64()) {
            return 0;
        }
        return d.qw;
    }

    const std::wstring &RegValue::toString() const {
        static std::wstring empty;

        assert(s);
        if (isMultiString()) {
            if (s->s.index() == 0) {
                s->convertMultiStringToVector();
            }
            return std::get<2>(s->s);
        }
        if (s && s->s.index() == 2) {
            return std::get<2>(s->s);
        }
        return empty;
    }

    const std::vector<std::wstring> &RegValue::toMultiString() const {
        static std::vector<std::wstring> empty;

        assert(s);
        if (isMultiString()) {
            return empty;
        }
        if (!s->ms) {
            s->convertVectorToMultiString();
        }
        return s->ms.value();
    }

    std::wstring RegValue::toExpandString() const {
        if (!isExpandString()) {
            return {};
        }
        return winapi::kernel32::ExpandEnvironmentStringsW(std::get<2>(s->s).c_str(), nullptr);
    }

    std::wstring RegValue::toLink() const {
        if (!isLink()) {
            return {};
        }
        return toString();
    }

    /*!
        \class RegKey
        \brief A registry key holder.
    */

    /*!
        Destructor. Closes the registry key if it was opened by this object.
    */
    RegKey::~RegKey() {
        if (_hkey && _owns) {
            std::ignore = RegCloseKey(_hkey);
        }
    }

    RegKey::RegKey(RegKey &&RHS) noexcept = default;

    RegKey &RegKey::operator=(RegKey &&RHS) noexcept = default;

    RegKey RegKey::open(const std::wstring &path, int access) {
        _ec.clear();

        HKEY hKey = nullptr;
        LSTATUS lStatus = RegOpenKeyExW(_hkey, path.c_str(), 0, static_cast<REGSAM>(access), &hKey);
        if (lStatus != ERROR_SUCCESS) {
            _ec = std::error_code(lStatus, stdc::windows_utf8_category());
            return {};
        }
        return RegKey(hKey, true);
    }

    RegKey RegKey::create(const std::wstring &path, int access, int options,
                          LPSECURITY_ATTRIBUTES sa, bool *exists) {
        HKEY hKey = nullptr;
        LSTATUS lStatus =
            RegCreateKeyExW(_hkey, path.c_str(), 0, nullptr, static_cast<DWORD>(options),
                            static_cast<REGSAM>(access), sa, &hKey, nullptr);
        if (lStatus != ERROR_SUCCESS) {
            _ec = std::error_code(lStatus, stdc::windows_utf8_category());
            return {};
        }
        return RegKey(hKey, true);
    }

    bool RegKey::close() {
        LSTATUS lStatus = RegCloseKey(_hkey);
        if (lStatus != ERROR_SUCCESS) {
            _ec = std::error_code(lStatus, stdc::windows_utf8_category());
            return false;
        }
        _hkey = nullptr;
        return true;
    }

    bool RegKey::flush() {
        return {};
    }

    bool RegKey::save(const std::wstring &filename) {
        return {};
    }

    bool RegKey::hasDirectory(const std::wstring &path) const {
        return {};
    }

    bool RegKey::hasValue(const std::wstring &name) const {
        return {};
    }

    RegValue RegKey::value(const std::wstring &name) const {
        return {};
    }

    bool RegKey::setValue(const std::wstring &name, const RegValue &value) {
        return {};
    }

    bool RegKey::remove(const std::wstring &subkey) {
        return {};
    }

    bool RegKey::remove() {
        return {};
    }

    bool RegKey::notify(HANDLE event, bool watchSubtree, int notifyFilter, bool async) {
        return {};
    }

    bool RegKey::key_iterator::fetch_next() {
        _data.name.clear();
        _data.ftLastWriteTime = {0, 0};

        if (!_hkey) {
            return false;
        }

        if (_maxsize == 0) {
            DWORD dwLongestSubKeyLen = 0;
            LSTATUS lStatus = RegQueryInfoKeyW(_hkey,   // Key handle
                                               nullptr, // Buffer for registry ked class name
                                               nullptr, // Size of class string
                                               nullptr, // Reserved
                                               nullptr, // Number of key subkeys
                                               &dwLongestSubKeyLen, // Longest subkey size
                                               nullptr,             // Longest class string
                                               nullptr,             // number of values for this key
                                               nullptr,             // Longest value name
                                               nullptr,             // Longest value data
                                               nullptr,             // Security descriptor
                                               nullptr              // Last key write time
            );
            if (lStatus != ERROR_SUCCESS) {
                _data.ec = std::error_code(lStatus, stdc::windows_utf8_category());
                return false;
            }
            _maxsize = dwLongestSubKeyLen;
        }

        std::wstring buffer;
        buffer.resize(_maxsize + 1);
        DWORD bufferSize = _maxsize + 1;
        LSTATUS status = RegEnumKeyExW(_hkey, _index, buffer.data(), &bufferSize, nullptr, nullptr,
                                       nullptr, &_data.ftLastWriteTime);
        if (status == ERROR_NO_MORE_ITEMS) {
            _hkey = nullptr; // Convert to end iterator
            _index = 0;
            return false;
        }
        if (status != ERROR_SUCCESS) {
            _data.ec = std::error_code(status, stdc::windows_utf8_category());
            return false;
        }

        // assign name
        buffer.reserve(bufferSize);
        _data.name = std::move(buffer);
        return true;
    }

    bool RegKey::value_iterator::fetch_next() {
        _data.name.clear();
        _data.value = {};

        if (!_hkey) {
            return false;
        }

        if (_maxsize == 0) {
            DWORD dwLongestValueNameLen = 0;
            LSTATUS lStatus = RegQueryInfoKeyW(_hkey,   // Key handle
                                               nullptr, // Buffer for registry ked class name
                                               nullptr, // Size of class string
                                               nullptr, // Reserved
                                               nullptr, // Number of key subkeys
                                               nullptr, // Longest subkey size
                                               nullptr, // Longest class string
                                               nullptr, // number of values for this key
                                               &dwLongestValueNameLen, // Longest value name
                                               nullptr,                // Longest value data
                                               nullptr,                // Security descriptor
                                               nullptr                 // Last key write time
            );
            if (lStatus != ERROR_SUCCESS) {
                _data.ec = std::error_code(lStatus, stdc::windows_utf8_category());
                return false;
            }
            _maxsize = dwLongestValueNameLen;
        }

        std::wstring buffer;
        buffer.resize(_maxsize + 1);
        DWORD bufferSize = _maxsize + 1;
        LSTATUS status = RegEnumValueW(_hkey, _index, buffer.data(), &bufferSize, nullptr, nullptr,
                                       nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS) {
            _hkey = nullptr; // Convert to end iterator
            _index = 0;
            _query = false;
            return false;
        }
        if (status != ERROR_SUCCESS) {
            _data.ec = std::error_code(status, stdc::windows_utf8_category());
            return false;
        }

        // assign name
        buffer.reserve(bufferSize);
        _data.name = std::move(buffer);

        // assign value
        if (_query) {
            RegKey key(_hkey);
            RegValue val = key.value(_data.name);
            if (!val.isValid()) {
                _data.ec = key.error_code();
                return false;
            }
            _data.value = val;
        }
        return true;
    }

}