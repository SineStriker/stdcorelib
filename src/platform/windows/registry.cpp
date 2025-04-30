#include "registry.h"

#include <optional>
#include <variant>

namespace stdc::winapi {

    struct RegValue::str {
        mutable std::variant<std::monostate, std::vector<uint8_t>, std::wstring> s;
        mutable std::optional<std::vector<std::wstring>> ms;
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

    RegValue::RegValue(int32_t value) : t(Int32) {
        dw = value;
    }

    RegValue::RegValue(int64_t value) : t(Int64) {
        qw = value;
    }

    RegValue::RegValue(const std::wstring &value, Type type) : t(type), s(std::make_shared<str>()) {
        s->s = value;
    }

    RegValue::RegValue(const wchar_t *value, int size, Type type)
        : t(type), s(std::make_shared<str>()) {
        s->s = size < 0 ? std::wstring(value) : std::wstring(value, size);
    }

    RegValue::RegValue(const std::vector<std::wstring> &value)
        : t(MultiString), s(std::make_shared<str>()) {
        s->ms = value;
    }

    RegValue::~RegValue() = default;

    RegValue::RegValue(const RegValue &RHS) = default;

    RegValue::RegValue(RegValue &&RHS) noexcept = default;

    RegValue &RegValue::operator=(const RegValue &RHS) = default;

    RegValue &RegValue::operator=(RegValue &&RHS) noexcept = default;

    int32_t RegValue::toInt32() const {
        if (!isInt32()) {
            return 0;
        }
        return dw;
    }

    int64_t RegValue::toInt64() const {
        if (!isInt64()) {
            return 0;
        }
        return qw;
    }

    std::wstring RegValue::toString() const {
        if (s && s->s.index() == 2) {
            if (isMultiString() && s->s.index() == 0) {
                // construct s
            }
            return std::get<2>(s->s);
        }
        return {};
    }

    const std::vector<std::wstring> &RegValue::toMultiString() const {
        static std::vector<std::wstring> empty;
        if (isMultiString()) {
            return empty;
        }
        if (!s->ms) {
            // construct ms
        }
        return s->ms.value();
    }

    std::wstring RegValue::toExpandString() const {
        if (!isExpandString()) {
            return {};
        }
        return kernel32::ExpandEnvironmentStringsW((wchar_t *) std::get<2>(s->s).c_str(), nullptr);
    }

    std::wstring RegValue::toLink() const {
        return toString();
    }

    /*!
        \class RegKey
        \brief A registry key holder.
    */
    RegKey::RegKey() : _key(nullptr) {
    }

    RegKey::~RegKey() {
        if (_key && !((_key >= HKEY_CLASSES_ROOT) &&
#if (WINVER >= 0x0400)
                      (_key <= HKEY_CURRENT_USER_LOCAL_SETTINGS)
#else
                      (_key <= HKEY_PERFORMANCE_DATA)
#endif
                          )) {
            RegCloseKey(_key);
        }
    }

    RegKey::RegKey(RegKey &&RHS) noexcept = default;

    RegKey &RegKey::operator=(RegKey &&RHS) noexcept = default;

    RegKey RegKey::open(const std::wstring &path, int access) {
        return {};
    }

    bool RegKey::close() {
        return {};
    }

    bool RegKey::flush() {
        return {};
    }

    bool RegKey::save(const std::wstring &filename) {
        return {};
    }

    bool RegKey::hasKey(const std::wstring &path) const {
        return {};
    }

    bool RegKey::hasValue(const std::wstring &name) const {
        return {};
    }

    RegValue RegKey::getValue(const std::wstring &name) const {
        return {};
    }

    bool RegKey::setValue(const std::wstring &name, const RegValue &value) {
        return {};
    }

    RegValue RegKey::getValue() const {
        return {};
    }

    bool RegKey::setValue(const RegValue &value) {
        return {};
    }

    RegKey RegKey::create(const std::wstring &path, int access, int options) {
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

}