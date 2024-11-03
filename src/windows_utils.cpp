#include "windows_utils.h"

#include <tuple>

namespace stdc {

    /*!
        Get formatted error message using \c FormatMessageW without ending line break.
    */
    std::wstring winFormatError(DWORD error, DWORD languageId, va_list *args) {
        std::wstring rc;
        wchar_t *lpMsgBuf;

        DWORD len = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                         FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, error, languageId, //
                                     reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, args);

        if (len) {
            // Remove trailing line breaks
            if (lpMsgBuf[len - 1] == L'\n') {
                lpMsgBuf[len - 1] = L'\0';
                len--;
                if (len > 1 && lpMsgBuf[len - 1] == L'\r') {
                    lpMsgBuf[len - 1] = L'\0';
                    len--;
                }
            }
            rc = std::wstring(lpMsgBuf, int(len));
            ::LocalFree(lpMsgBuf);
        } else {
            rc += L"unknown error";
        }
        return rc;
    }

    /*!
        Get the full path of \c GetDllDirectoryW result.
    */
    std::wstring winGetFullDllDirectory() {
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

    /*!
        Get the full path of \c GetModuleFileNameW result.
    */
    std::wstring winGetFullModuleFileName(HMODULE hModule) {
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

    /*!
        Returns multi-byte string converted from wide string.
    */
    std::wstring win8bitToWide(const std::string_view &s, UINT cp, DWORD flags) {
        const int utf16Length = ::MultiByteToWideChar(cp, flags, s.data(), s.size(), nullptr, 0);
        if (utf16Length <= 0) {
            return {};
        }
        std::wstring utf16Str;
        utf16Str.resize(utf16Length);
        std::ignore =
            ::MultiByteToWideChar(cp, flags, s.data(), s.size(), utf16Str.data(), utf16Length);
        return utf16Str;
    }

    /*!
        Returns wide string converted from multi-byte string.
    */
    std::string winWideTo8bit(const std::wstring_view &s, UINT cp, DWORD flags) {
        const int utf8Length =
            ::WideCharToMultiByte(cp, flags, s.data(), s.size(), nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0) {
            return {};
        }
        std::string utf8Str;
        utf8Str.resize(utf8Length);
        std::ignore = ::WideCharToMultiByte(cp, flags, s.data(), s.size(), utf8Str.data(),
                                            utf8Length, nullptr, nullptr);
        return utf8Str;
    }

}