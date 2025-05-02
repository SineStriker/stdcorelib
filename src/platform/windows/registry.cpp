#include "registry.h"

#include <variant>
#include <cassert>

#include "winapi.h"
#include "str.h"
#include "console.h"

#include "3rdparty/llvm/smallvector.h"

namespace stdc::windows {

    template <class T>
    static T qFromLittleEndian(const uint8_t *data) {
        // TODO: support when the host system is not little-endian
        return *reinterpret_cast<const T *>(data);
    }

    template <class T>
    static T qFromBigEndian(const uint8_t *data) {
        // TODO: implement
        assert(false);
        return {};
    }

    static inline std::error_code make_status_error_code(LSTATUS status) {
        return std::error_code(status, stdc::windows_utf8_category());
    }

    static inline LSTATUS getRegSubKeyCountAndMaxLen(HKEY hkey, DWORD *dwSubKeys,
                                                     DWORD *dwLongestSubKeyLen) {
        return RegQueryInfoKeyW(hkey,               // Key handle
                                nullptr,            // Buffer for registry ked class name
                                nullptr,            // Size of class string
                                nullptr,            // Reserved
                                dwSubKeys,          // Number of key subkeys
                                dwLongestSubKeyLen, // Longest subkey size
                                nullptr,            // Longest class string
                                nullptr,            // number of values for this key
                                nullptr,            // Longest value name
                                nullptr,            // Longest value data
                                nullptr,            // Security descriptor
                                nullptr             // Last key write time
        );
    }

    static inline LSTATUS getRegValueCountAndMaxLen(HKEY hkey, DWORD *dwValues,
                                                    DWORD *dwLongestValueNameLen) {
        return RegQueryInfoKeyW(hkey,                  // Key handle
                                nullptr,               // Buffer for registry ked class name
                                nullptr,               // Size of class string
                                nullptr,               // Reserved
                                nullptr,               // Number of key subkeys
                                nullptr,               // Longest subkey size
                                nullptr,               // Longest class string
                                dwValues,              // number of values for this key
                                dwLongestValueNameLen, // Longest value name
                                nullptr,               // Longest value data
                                nullptr,               // Security descriptor
                                nullptr                // Last key write time
        );
    }

    struct RegValue::Comp {
        mutable std::variant<std::monostate, std::vector<uint8_t>, std::wstring> s;
        mutable std::optional<std::vector<std::wstring>> ms;

        void s2ms() const {
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

        void ms2s() const {
            assert(ms);

            std::wstring env_str;
            for (const auto &item : std::as_const(ms.value())) {
                env_str += item;
                env_str.push_back(L'\0');
            }
            env_str.push_back(L'\0');
            s = std::move(env_str);
        }
    };

    /*!
        \class RegValue
        \brief A registry value holder.
    */

    RegValue::RegValue(Type type) : t(type) {
        switch (type) {
            case None:
            case Invalid:
                break;
            case Binary:
                comp = std::make_shared<Comp>();
                comp->s = std::vector<uint8_t>();
                break;
            case Int32:
                d.dw = 0;
                break;
            case Int64:
                d.qw = 0;
                break;
            case String:
            case ExpandString:
            case Link:
                comp = std::make_shared<Comp>();
                comp->s = std::wstring();
                break;
            case MultiString:
                comp = std::make_shared<Comp>();
                comp->ms = std::vector<std::wstring>();
                break;
            default:
                d.p = nullptr;
                break;
        }
    }

    RegValue::RegValue(const uint8_t *data, int size) : t(Binary), comp(std::make_shared<Comp>()) {
        comp->s = std::vector<uint8_t>(data, data + size);
    }

    RegValue::RegValue(std::vector<uint8_t> &&data, int size)
        : t(Binary), comp(std::make_shared<Comp>()) {
        comp->s = std::move(data);
    }

    RegValue::RegValue(int32_t value) : t(Int32) {
        d.dw = value;
    }

    RegValue::RegValue(int64_t value) : t(Int64) {
        d.qw = value;
    }

    RegValue::RegValue(const std::wstring &value, Type type)
        : t(type), comp(std::make_shared<Comp>()) {
        comp->s = value;
    }

    RegValue::RegValue(std::wstring &&value, Type type) : t(type), comp(std::make_shared<Comp>()) {
        comp->s = std::move(value);
    }

    RegValue::RegValue(const wchar_t *value, int size, Type type)
        : t(type), comp(std::make_shared<Comp>()) {
        comp->s = size < 0 ? std::wstring(value) : std::wstring(value, size);
    }

    RegValue::RegValue(const std::vector<std::wstring> &value)
        : t(MultiString), comp(std::make_shared<Comp>()) {
        comp->ms = value;
    }

    RegValue::RegValue(std::vector<std::wstring> &&value)
        : t(MultiString), comp(std::make_shared<Comp>()) {
        comp->ms = std::move(value);
    }

    RegValue::RegValue(const void *data, int type) : t(type) {
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
        assert(comp && comp->s.index() == 1);
        return std::get<1>(comp->s);
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

        if (isMultiString()) {
            assert(comp && (comp->s.index() == 2 || comp->ms));
            if (comp->s.index() == 0) {
                comp->ms2s();
            }
            return std::get<2>(comp->s);
        }
        if (comp && comp->s.index() == 2) {
            return std::get<2>(comp->s);
        }
        return empty;
    }

    const std::vector<std::wstring> &RegValue::toMultiString() const {
        static std::vector<std::wstring> empty;

        if (!isMultiString()) {
            return empty;
        }
        assert(comp && (comp->s.index() == 2 || comp->ms));
        if (!comp->ms) {
            comp->s2ms();
        }
        return comp->ms.value();
    }

    std::wstring RegValue::toExpandString() const {
        if (!isExpandString()) {
            return {};
        }
        assert(comp && comp->s.index() == 2);
        return winapi::kernel32::ExpandEnvironmentStringsW(std::get<2>(comp->s).c_str(), nullptr);
    }

    std::wstring RegValue::toLink() const {
        if (!isLink()) {
            return {};
        }
        assert(comp && comp->s.index() == 2);
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

    static inline HKEY getReservedKey(RegKey::ReservedKey key) {
        switch (key) {
            case RegKey::RK_ClassesRoot:
                return HKEY_CLASSES_ROOT;
            case RegKey::RK_CurrentUser:
                return HKEY_CURRENT_USER;
            case RegKey::RK_LocalMachine:
                return HKEY_LOCAL_MACHINE;
            case RegKey::RK_Users:
                return HKEY_USERS;
            case RegKey::RK_CurrentConfig:
                return HKEY_CURRENT_CONFIG;
            default:
                break;
        }
        return nullptr;
    }

    RegKey::RegKey(ReservedKey key) noexcept : _hkey(getReservedKey(key)), _owns(false) {
    }

    RegKey::RegKey(RegKey &&RHS) noexcept
        : _hkey(RHS._hkey), _owns(RHS._owns), _max_key_name_size(RHS._max_key_name_size),
          _max_value_name_size(RHS._max_value_name_size) {
        RHS._hkey = nullptr;
        RHS._owns = false;
    }

    RegKey &RegKey::operator=(RegKey &&RHS) noexcept = default;

    RegKey RegKey::open(const std::wstring &path, std::error_code &ec, int access) noexcept {
        ec.clear();

        HKEY hkey = nullptr;
        LSTATUS status = RegOpenKeyExW(_hkey, path.c_str(), 0, static_cast<REGSAM>(access), &hkey);
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return {};
        }
        return RegKey(hkey, true);
    }

    RegKey RegKey::create(const std::wstring &path, std::error_code &ec, int access, int options,
                          LPSECURITY_ATTRIBUTES sa, bool *exists) noexcept {
        ec.clear();

        HKEY hkey = nullptr;
        LSTATUS status =
            RegCreateKeyExW(_hkey, path.c_str(), 0, nullptr, static_cast<DWORD>(options),
                            static_cast<REGSAM>(access), sa, &hkey, nullptr);
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return {};
        }
        return RegKey(hkey, true);
    }

    bool RegKey::close(std::error_code &ec) noexcept {
        ec.clear();

        LSTATUS status = RegCloseKey(_hkey);
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return false;
        }
        _hkey = nullptr;
        return true;
    }

    int RegKey::keyCount(std::error_code &ec) const noexcept {
        ec.clear();

        // update max key name size by the way
        DWORD count;
        LSTATUS status = getRegSubKeyCountAndMaxLen(_hkey, &count, &_max_key_name_size);
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return 0;
        }
        return count;
    }

    std::optional<RegKey::KeyData> RegKey::keyAt(int index, std::error_code &ec) const noexcept {
        ec.clear();

        auto &maxsize = _max_value_name_size;
        KeyData data;

        LSTATUS status;
        std::wstring buffer;
        while (true) {
            buffer.resize(maxsize + 1);

            DWORD bufferSize = maxsize + 1;
            status = RegEnumKeyExW(_hkey, index, buffer.data(), &bufferSize, nullptr, nullptr,
                                   nullptr, &data.lastWriteTime);
            if (status == ERROR_MORE_DATA) {
                status = getRegSubKeyCountAndMaxLen(_hkey, nullptr, &maxsize);
                if (status != ERROR_SUCCESS) {
                    ec = make_status_error_code(status);
                    return {};
                }
                continue;
            }
            if (status != ERROR_SUCCESS) {
                ec = make_status_error_code(status);
                return {};
            }
            buffer.resize(bufferSize);
            break;
        };

        // assign name
        data.name = std::move(buffer);
        return data;
    }

    int RegKey::valueCount(std::error_code &ec) const noexcept {
        ec.clear();

        // update max value name size by the way
        DWORD count;
        LSTATUS status = getRegValueCountAndMaxLen(_hkey, &count, &_max_value_name_size);
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return 0;
        }
        return count;
    }

    std::optional<RegKey::ValueData> RegKey::valueAt(int index, std::error_code &ec,
                                                     bool query) const noexcept {
        ec.clear();

        auto &maxsize = _max_value_name_size;
        RegKey::ValueData data;

        LSTATUS status;
        std::wstring buffer;
        while (true) {
            buffer.resize(maxsize + 1);

            DWORD bufferSize = maxsize + 1;
            status = RegEnumValueW(_hkey, index, buffer.data(), &bufferSize, nullptr, nullptr,
                                   nullptr, nullptr);
            if (status == ERROR_MORE_DATA) {
                status = getRegValueCountAndMaxLen(_hkey, nullptr, &maxsize);
                if (status != ERROR_SUCCESS) {
                    ec = make_status_error_code(status);
                    return {};
                }
                continue;
            }
            if (status != ERROR_SUCCESS) {
                ec = make_status_error_code(status);
                return {};
            }
            buffer.resize(bufferSize);
            break;
        };

        // assign name
        data.name = std::move(buffer);

        // assign value
        if (query) {
            RegValue val = value(data.name);
            if (!val.isValid()) {
                return {};
            }
            data.value = val;
        }
        return data;
    }

    bool RegKey::flush(std::error_code &ec) noexcept {
        ec.clear();

        LSTATUS status = RegFlushKey(_hkey);
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return false;
        }
        return true;
    }

    bool RegKey::save(const std::wstring &filename, std::error_code &ec, LPSECURITY_ATTRIBUTES sa,
                      int flags) noexcept {
        ec.clear();

        LSTATUS status = RegSaveKeyExW(_hkey, filename.c_str(), sa, flags);
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return false;
        }
        return true;
    }

    bool RegKey::hasKey(const std::wstring &path, std::error_code &ec) const noexcept {
        ec.clear();

        bool hasKey = false;
        HKEY hkey = nullptr;
        LSTATUS status = RegOpenKeyExW(_hkey, path.c_str(), 0, KEY_READ, &hkey);
        if (status == ERROR_SUCCESS) {
            assert(hkey);
            RegCloseKey(hkey);
            hasKey = true;
        } else if (status != ERROR_FILE_NOT_FOUND) {
            ec = make_status_error_code(status);
            return false;
        }
        return hasKey;
    }

    bool RegKey::hasValue(const std::wstring &name, std::error_code &ec) const noexcept {
        ec.clear();

        bool hasValue = false;
        LSTATUS status = RegQueryValueExW(_hkey, name.c_str(), nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_SUCCESS) {
            hasValue = true;
        } else if (status != ERROR_FILE_NOT_FOUND) {
            ec = make_status_error_code(status);
            return false;
        }
        return hasValue;
    }

    // https://github.com/qt/qtbase/blob/v6.8.0/src/corelib/kernel/qwinregistry.cpp#L39
    RegValue RegKey::value(const std::wstring &name, std::error_code &ec) const noexcept {
        ec.clear();

        // NOTE: Empty value name is allowed in Windows registry, it means the default
        // or unnamed value of a key, you can read/write/delete such value normally.

        // Use nullptr when we need to access the default value.
        const auto subKeyC =
            name.empty() ? nullptr : reinterpret_cast<const wchar_t *>(name.data());

        // Get the size and type of the value.
        DWORD dataType = REG_NONE;
        DWORD dataSize = 0;
        LONG ret = RegQueryValueExW(_hkey, subKeyC, nullptr, &dataType, nullptr, &dataSize);
        if (ret != ERROR_SUCCESS)
            return RegValue(RegValue::Invalid);

        // Get the value.
        llvm::SmallVector<unsigned char, 512> data(dataSize);
        std::fill(data.data(), data.data() + dataSize, 0u);

        ret = RegQueryValueExW(_hkey, subKeyC, nullptr, nullptr, data.data(), &dataSize);
        if (ret != ERROR_SUCCESS) {
            ec = make_status_error_code(ret);
            return RegValue(RegValue::Invalid);
        }

        switch (dataType) {
            case REG_SZ: {
                if (dataSize > 0) {
                    return RegValue(reinterpret_cast<const wchar_t *>(data.data()),
                                    dataSize / sizeof(wchar_t));
                }
                return RegValue(RegValue::Type::String);
            }
            case REG_EXPAND_SZ: {
                if (dataSize > 0) {
                    return RegValue(reinterpret_cast<const wchar_t *>(data.data()),
                                    dataSize / sizeof(wchar_t), RegValue::ExpandString);
                }
                return RegValue(RegValue::Type::ExpandString);
            }

            case REG_MULTI_SZ: {
                if (dataSize > 0) {
                    return RegValue(reinterpret_cast<const wchar_t *>(data.data()),
                                    dataSize / sizeof(wchar_t), RegValue::MultiString);
                }
                return RegValue(RegValue::MultiString);
            }

            case REG_NONE: {
                return RegValue(RegValue::None);
            }
            case REG_BINARY: {
                if (dataSize > 0) {
                    return RegValue(reinterpret_cast<const uint8_t *>(data.data()), dataSize);
                }
                return RegValue(RegValue::Type::Binary);
            }

            case REG_DWORD: // Same as REG_DWORD_LITTLE_ENDIAN
                return qFromLittleEndian<uint32_t>(data.data());

            case REG_DWORD_BIG_ENDIAN:
                return qFromBigEndian<uint32_t>(data.data());

            case REG_QWORD: // Same as REG_QWORD_LITTLE_ENDIAN
                return qFromLittleEndian<uint64_t>(data.data());

            default:
                break;
        }
        return RegValue(RegValue::Invalid);
    }

    bool RegKey::setValue(const std::wstring &name, const RegValue &value,
                          std::error_code &ec) noexcept {
        if (!value.isValid()) {
            return removeValue(name, ec);
        }

        ec.clear();

        // TODO
        return {};
    }

    bool RegKey::removeKey(const std::wstring &path, std::error_code &ec) noexcept {
        ec.clear();

        LSTATUS status = RegDeleteTreeW(_hkey, path.c_str());
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return false;
        }
        return true;
    }

    bool RegKey::removeValue(const std::wstring &name, std::error_code &ec) noexcept {
        ec.clear();

        LSTATUS status = RegDeleteValueW(_hkey, name.c_str());
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return false;
        }
        return true;
    }

    bool RegKey::remove(std::error_code &ec) noexcept {
        ec.clear();

        LSTATUS status = RegDeleteTreeW(_hkey, nullptr);
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return false;
        }
        return true;
    }

    bool RegKey::notify(std::error_code &ec, bool watchSubtree, int notifyFilter, HANDLE event,
                        bool async) noexcept {
        ec.clear();

        LSTATUS status = RegNotifyChangeKeyValue(_hkey, watchSubtree, notifyFilter, event, async);
        if (status != ERROR_SUCCESS) {
            ec = make_status_error_code(status);
            return false;
        }
        return true;
    }

}