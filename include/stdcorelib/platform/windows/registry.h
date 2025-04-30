#ifndef STDCORELIB_REGISTRY_H
#define STDCORELIB_REGISTRY_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

#include <stdcorelib/platform/windows/winapi.h>

namespace stdc::winapi {

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
        RegValue(const uint8_t *data, size_t size);
        RegValue(int32_t value);
        RegValue(int64_t value);
        RegValue(const std::string &value, Type type = String);
        RegValue(const char *value, Type type = String);
        RegValue(const std::vector<std::string> &value);
        RegValue(const void *data, Type type);
        ~RegValue();

        RegValue(const RegValue &RHS);
        RegValue(RegValue &&RHS) noexcept;
        RegValue &operator=(const RegValue &RHS);
        RegValue &operator=(RegValue &&RHS) noexcept;

    public:
        inline int type() const {
            return t;
        }

        int32_t toInt32() const;
        int64_t toInt64() const;
        std::string toString() const;
        const std::vector<std::string> &toMultiString() const;
        std::string toExpandString() const;
        std::string toLink() const;

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

    protected:
        int t;
        union {
            int32_t dw;
            int64_t qw;
            void *p;
        };
        struct str;
        std::shared_ptr<str> s;
    };

    class STDCORELIB_EXPORT RegKey {
    public:
        enum AccessPermission {
            AP_Delete = DELETE,
            AP_ReadControl = READ_CONTROL,
            AP_WriteDAC = WRITE_DAC,
            AP_WriteOwner = WRITE_OWNER,
            AP_AllAccess = KEY_ALL_ACCESS,
            AP_CreateSubKey = KEY_CREATE_SUB_KEY,
            AP_EnumerateSubKeys = KEY_ENUMERATE_SUB_KEYS,
            AP_Notify = KEY_NOTIFY,
            AP_QueryValue = KEY_QUERY_VALUE,
            AP_SetValue = KEY_SET_VALUE,
            AP_Wow6432 = KEY_WOW64_32KEY,
            AP_Wow6464 = KEY_WOW64_64KEY,
            AP_Read = KEY_READ,
            AP_Write = KEY_WRITE,
            AP_Execute = KEY_EXECUTE,
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

        RegKey();
        ~RegKey();

        RegKey(RegKey &&RHS) noexcept;
        RegKey &operator=(RegKey &&RHS) noexcept;

    public:
        RegKey open(const std::wstring &path, int access = AP_Read);
        bool close();

        bool flush();
        bool save(const std::wstring &filename);

        bool hasKey(const std::wstring &path) const;
        bool hasValue(const std::wstring &name) const;

        RegValue getValue(const std::wstring &name) const;
        bool setValue(const std::wstring &name, const RegValue &value);

        RegValue getValue() const;
        bool setValue(const RegValue &value);

        RegKey create(const std::wstring &path, int access = AP_Read, int options = CO_NonVolatile);
        bool remove(const std::wstring &subkey);
        bool remove();

        bool notify(HANDLE event = nullptr, bool watchSubtree = false,
                    int notifyFilter = NF_ChangeName | NF_ChangeAttributes, bool async = false);

        inline HKEY handle() const {
            return _key;
        }

    protected:
        HKEY _key;

        STDCORELIB_DISABLE_COPY(RegKey);
    };

}

#endif // STDCORELIB_REGISTRY_H