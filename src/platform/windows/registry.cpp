#include "registry.h"

#include <optional>

#include "pimpl.h"

namespace stdc::winapi {

    struct RegValue::str {
        std::vector<char> s;
        mutable std::optional<std::vector<std::string>> ms;
    };

    RegValue::RegValue(Type type) : t(Invalid) {
    }

    RegValue::RegValue(const uint8_t *data, size_t size) {
    }

    RegValue::RegValue(int32_t value) {
    }

    RegValue::RegValue(int64_t value) {
    }

    RegValue::RegValue(const std::string &value, Type type) {
    }

    RegValue::RegValue(const char *value, Type type) {
    }

    RegValue::RegValue(const std::vector<std::string> &value) {
    }

    RegValue::~RegValue() = default;

    RegValue::RegValue(const RegValue &RHS) = default;

    RegValue::RegValue(RegValue &&RHS) noexcept = default;

    RegValue &RegValue::operator=(const RegValue &RHS) = default;

    RegValue &RegValue::operator=(RegValue &&RHS) noexcept = default;

    int32_t RegValue::toInt32() const {
        return {};
    }

    int64_t RegValue::toInt64() const {
        return {};
    }

    std::string RegValue::toString() const {
        return {};
    }

    const std::vector<std::string> &RegValue::toMultiString() const {
        static std::vector<std::string> empty;
        if (isMultiString()) {
            return empty;
        }
        if (!s->ms) {
            // construct ms
        }
        return s->ms.value();
    }

    std::string RegValue::toExpandString() const {
        return {};
    }

    std::string RegValue::toLink() const {
        return {};
    }

    RegKey::RegKey() : _key(nullptr) {
    }

    RegKey::~RegKey() = default;

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