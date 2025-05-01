#ifndef STDCORELIB_REGISTRY_H
#define STDCORELIB_REGISTRY_H

#include "stdc_windows.h"

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <system_error>
#include <optional>

#include <stdcorelib/stdc_global.h>

namespace stdc::windows {

    class STDCORELIB_EXPORT RegValue {
    public:
        enum Type {
            Invalid = -1,
            None = REG_NONE,
            Binary = REG_BINARY,
            Int32 = REG_DWORD, // DWORD
            Int64 = REG_QWORD, // QWORD
            String = REG_SZ,
            MultiString = REG_MULTI_SZ,
            ExpandString = REG_EXPAND_SZ,
            Link = REG_LINK,
        };

        RegValue(Type type = None);
        RegValue(const uint8_t *data, int size);
        RegValue(std::vector<uint8_t> &&data, int size);
        RegValue(int32_t value);
        RegValue(int64_t value);
        RegValue(const std::wstring &value, Type type = String);
        RegValue(std::wstring &&value, Type type = String);
        RegValue(const wchar_t *value, int size = -1, Type type = String);
        RegValue(const std::vector<std::wstring> &value);
        RegValue(std::vector<std::wstring> &&value);
        RegValue(const void *data, int type); // data shouldn't be deleted before RegValue destructs
        ~RegValue();

        RegValue(const RegValue &RHS);
        RegValue(RegValue &&RHS) noexcept;
        RegValue &operator=(const RegValue &RHS);
        RegValue &operator=(RegValue &&RHS) noexcept;

    public:
        inline int type() const {
            return t;
        }

        const std::vector<uint8_t> &toBinary() const;
        int32_t toInt32() const;
        int64_t toInt64() const;
        const std::wstring &toString() const;
        const std::vector<std::wstring> &toMultiString() const;
        std::wstring toExpandString() const;
        std::wstring toLink() const;

        inline bool isValid() const {
            return type() != Invalid;
        }
        inline bool isNone() const {
            return type() == None;
        }
        inline bool isBinary() const {
            return type() == Binary;
        }
        inline bool isInt32() const {
            return type() == Int32;
        }
        inline bool isInt64() const {
            return type() == Int64;
        }
        inline bool isString() const {
            return type() == String;
        }
        inline bool isMultiString() const {
            return type() == MultiString;
        }
        inline bool isExpandString() const {
            return type() == ExpandString;
        }
        inline bool isLink() const {
            return type() == Link;
        }

        inline const void *dataPointer() const {
            return d.p;
        }

    protected:
        int t;
        union {
            int32_t dw;
            int64_t qw;
            const void *p;
        } d;
        struct comp;
        std::shared_ptr<comp> _comp;
    };

    class STDCORELIB_EXPORT RegKey {
    public:
        enum DesiredAccess {
            DA_Delete = DELETE,
            DA_ReadControl = READ_CONTROL,
            DA_WriteDAC = WRITE_DAC,
            DA_WriteOwner = WRITE_OWNER,
            DA_AllAccess = KEY_ALL_ACCESS,
            DA_CreateSubKey = KEY_CREATE_SUB_KEY,
            DA_EnumerateSubKeys = KEY_ENUMERATE_SUB_KEYS,
            DA_Notify = KEY_NOTIFY,
            DA_QueryValue = KEY_QUERY_VALUE,
            DA_SetValue = KEY_SET_VALUE,
            DA_Wow6432 = KEY_WOW64_32KEY,
            DA_Wow6464 = KEY_WOW64_64KEY,
            DA_Read = KEY_READ,
            DA_Write = KEY_WRITE,
            DA_Execute = KEY_EXECUTE,
        };

        enum CreateOptions {
            CO_BackupRestore = REG_OPTION_BACKUP_RESTORE,
            CO_NonVolatile = REG_OPTION_NON_VOLATILE,
            CO_Volatile = REG_OPTION_VOLATILE,
        };

        enum NotifyFilter {
            NF_ChangeName = REG_NOTIFY_CHANGE_NAME,
            NF_ChangeAttributes = REG_NOTIFY_CHANGE_ATTRIBUTES,
            NF_ChangeLastSet = REG_NOTIFY_CHANGE_LAST_SET,
            NF_ChangeSecurity = REG_NOTIFY_CHANGE_SECURITY,
            NF_ThreadAgnostic = REG_NOTIFY_THREAD_AGNOSTIC,
            NF_LegalChangeFilter = REG_LEGAL_CHANGE_FILTER,
        };

        struct KeyData {
            std::wstring name;
            FILETIME lastWriteTime{};
        };

        struct ValueData {
            std::wstring name;
            RegValue value;
        };

        // constructs from an existing HKEY handle
        inline RegKey(HKEY hkey = nullptr) noexcept : _hkey(hkey), _owns(false) {
        }
        ~RegKey();

        RegKey(RegKey &&RHS) noexcept;
        RegKey &operator=(RegKey &&RHS) noexcept;

    public:
        RegKey open(const std::wstring &path, int access = DA_Read);
        RegKey create(const std::wstring &path, int access = DA_Read, int options = CO_NonVolatile,
                      LPSECURITY_ATTRIBUTES sa = nullptr, bool *exists = nullptr);
        bool close();

        DWORD keyCount() const;
        std::optional<KeyData> keyAt(DWORD index) const;

        DWORD valueCount() const;
        std::optional<ValueData> valueAt(DWORD index, bool query = false) const;

        bool flush();
        bool save(const std::wstring &filename);

        bool hasKey(const std::wstring &path) const;
        bool hasValue(const std::wstring &name) const;

        RegValue value(const std::wstring &name) const;
        bool setValue(const std::wstring &name, const RegValue &value);

        bool removeKey(const std::wstring &path);
        bool removeValue(const std::wstring &name);
        bool remove();

        bool notify(HANDLE event = nullptr, bool watchSubtree = false,
                    int notifyFilter = NF_ChangeName | NF_ChangeAttributes, bool async = false);

        inline HKEY handle() const {
            return _hkey;
        }

        inline bool isValid() const {
            return _hkey != nullptr;
        }

        inline std::error_code errorCode() const {
            return _ec;
        }

        class key_enumerator;

        class key_iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = KeyData;
            using difference_type = ptrdiff_t;
            using pointer = const value_type *;
            using reference = const value_type &;

            // default constructor creates an invalid iterator
            inline key_iterator() noexcept : _key(nullptr), _index(0) {
            }

            inline reference operator*() const noexcept {
                return _data;
            }

            inline pointer operator->() const noexcept {
                return &_data;
            }

            inline key_iterator &operator++() {
                ++_index;
                fetch();
                return *this;
            }

            inline key_iterator operator++(int) {
                auto tmp = *this;
                ++*this;
                return tmp;
            }

            inline bool operator==(const key_iterator &RHS) const noexcept {
                return _key == RHS._key && _index == RHS._index;
            }

            inline bool operator!=(const key_iterator &RHS) const noexcept {
                return !(*this == RHS);
            }

        private:
            inline key_iterator(const RegKey *key, DWORD index = 0, int maxsize = 0) noexcept
                : _key(key), _index(index) {
                fetch();
            }

            STDCORELIB_EXPORT void fetch() const;

            const RegKey *_key;
            mutable DWORD _index;
            mutable value_type _data;

            friend class key_enumerator;
        };

        class key_enumerator {
        public:
            inline key_enumerator(const RegKey *key) : _key(key) {
            }
            inline key_iterator begin() const {
                return key_iterator(_key, 0);
            }
            inline key_iterator end() const {
                return key_iterator(_key, -1);
            }

        private:
            const RegKey *_key;
        };

        class value_enumerator;

        class value_iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = ValueData;
            using difference_type = ptrdiff_t;
            using pointer = const value_type *;
            using reference = const value_type &;

            // default constructor creates an end iterator
            inline value_iterator() noexcept : _key(nullptr), _index(0), _query(false) {
            }

            inline reference operator*() const noexcept {
                return _data;
            }

            inline pointer operator->() const noexcept {
                return &_data;
            }

            inline value_iterator &operator++() {
                ++_index;
                fetch();
                return *this;
            }

            inline value_iterator operator++(int) {
                auto tmp = *this;
                ++*this;
                return tmp;
            }

            inline bool operator==(const value_iterator &RHS) const noexcept {
                return _key == RHS._key && _index == RHS._index;
            }

            inline bool operator!=(const value_iterator &RHS) const noexcept {
                return !(*this == RHS);
            }

        private:
            inline value_iterator(const RegKey *key, DWORD index = 0, bool query = false) noexcept
                : _key(key), _query(query), _index(index) {
                fetch();
            }

            STDCORELIB_EXPORT void fetch() const;

            const RegKey *_key;
            bool _query;
            mutable DWORD _index;
            mutable value_type _data;

            friend class value_enumerator;
        };

        class value_enumerator {
        public:
            inline value_enumerator(const RegKey *key, bool query) : _key(key), _query(query) {
            }
            inline value_iterator begin() const {
                return value_iterator(_key, 0, _query);
            }
            inline value_iterator end() const {
                return value_iterator(_key, -1, _query);
            }

        private:
            const RegKey *_key;
            bool _query;
        };

        inline key_enumerator enumKeys() const {
            _ec.clear();
            return key_enumerator(this);
        }

        inline value_enumerator enumValues(bool query = false) const {
            _ec.clear();
            return value_enumerator(this, query);
        }

    protected:
        inline RegKey(HKEY hkey, bool owns) noexcept : _hkey(hkey), _owns(owns) {
        }

        HKEY _hkey;
        bool _owns;
        mutable DWORD _max_key_name_size = 0;
        mutable DWORD _max_value_name_size = 0;
        mutable std::error_code _ec;

        STDCORELIB_DISABLE_COPY(RegKey);

        friend class key_iterator;
        friend class value_iterator;
    };

}

#endif // STDCORELIB_REGISTRY_H