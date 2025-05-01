#include "winapi.h"

namespace stdc {

    /*!
        \namespace winapi
        \brief C++ wrapper for Windows API functions.
    */

}

namespace stdc::winapi {

    std::wstring kernel32::FormatMessageW(int from, LPCVOID source, DWORD message_id,
                                          DWORD language_id, bool ignore_inserts, void *arguments,
                                          bool is_array) {
        DWORD dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | from;
        if (ignore_inserts)
            dwFlags |= FORMAT_MESSAGE_IGNORE_INSERTS;
        if (is_array)
            dwFlags |= FORMAT_MESSAGE_ARGUMENT_ARRAY;

        std::wstring ret;
        wchar_t *pAllocated = nullptr;

        DWORD dwLength = ::FormatMessageW(dwFlags, source, message_id, language_id,
                                          (LPWSTR) &pAllocated, 0, (va_list *) arguments);
        if (dwLength != 0) {
            ret.assign(pAllocated, dwLength);
        }
        ::LocalFree((HLOCAL) pAllocated);
        return ret;
    }

    std::wstring kernel32::MultiByteToWideChar(UINT cp, DWORD flags, const std::string_view &s) {
        auto size = ::MultiByteToWideChar(cp, flags, s.data(), int(s.size()), nullptr, 0);
        if (size <= 0) {
            return {};
        }
        std::wstring res;
        res.resize(size);
        std::ignore = ::MultiByteToWideChar(cp, flags, s.data(), int(s.size()), res.data(), size);
        return res;
    }

    std::string kernel32::WideCharToMultiByte(UINT cp, DWORD flags, const std::wstring_view &s) {
        auto size =
            ::WideCharToMultiByte(cp, flags, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) {
            return {};
        }
        std::string res;
        res.resize(size);
        std::ignore = ::WideCharToMultiByte(cp, flags, s.data(), int(s.size()), res.data(), size,
                                            nullptr, nullptr);
        return res;
    }

    std::wstring kernel32::GetDllDirectoryW() {
        auto size = ::GetDllDirectoryW(0, nullptr);
        if (size == 0) {
            return {};
        }

        std::wstring res;
        res.resize(size);
        if (!::GetDllDirectoryW(size, res.data())) {
            return {};
        }
        return res;
    }

    std::wstring kernel32::GetModuleFileNameW(HMODULE hModule) {
        // https://stackoverflow.com/a/57114164/17177007
        DWORD size = MAX_PATH;
        std::wstring buffer;
        buffer.resize(size);
        while (true) {
            DWORD result = ::GetModuleFileNameW(hModule, buffer.data(), size);
            if (result == 0) {
                break;
            }

            if (result < size) {
                buffer.resize(result);
                return buffer;
            }

            // Check if a larger buffer is needed
            if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                size *= 2;
                buffer.resize(size);
                continue;
            }

            // Exactly
            return buffer;
        }
        return {};
    }

    std::wstring kernel32::GetEnvironmentVariableW(LPCWSTR name, bool *ok) {
        DWORD size = ::GetEnvironmentVariableW(name, nullptr, 0);
        if (size == 0) {
            if (ok)
                *ok = GetLastError() == ERROR_ENVVAR_NOT_FOUND;
            return {};
        }
        std::wstring res;
        res.resize(size - 1);
        if (::GetEnvironmentVariableW(name, res.data(), size) == 0) {
            if (ok)
                *ok = false;
            return {};
        }
        if (ok)
            *ok = true;
        return res;
    }

    std::wstring kernel32::ExpandEnvironmentStringsW(LPCWSTR src, bool *ok) {
        DWORD size = ::ExpandEnvironmentStringsW(src, nullptr, 0);
        if (size == 0) {
            if (ok)
                *ok = false;
            return {};
        }
        std::wstring res;
        res.resize(size);
        if (::ExpandEnvironmentStringsW(src, res.data(), size) == 0) {
            if (ok)
                *ok = false;
            return {};
        }
        if (ok)
            *ok = true;
        return res;
    }

}