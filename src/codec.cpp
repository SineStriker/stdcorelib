#include "codec.h"

#ifdef _WIN32
#  include <windows.h>
#endif

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace cpputils {

    std::string wideToUtf8(const wchar_t *s, int size) {
#ifdef _WIN32
        if (size < 0) {
            size = (int) wcslen(s);
        }
        if (size == 0) {
            return {};
        }
        const int utf8Length = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s, size,
                                                     nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0) {
            return {};
        }
        std::string utf8Str;
        utf8Str.resize(utf8Length);
        ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s, size, utf8Str.data(), utf8Length,
                              nullptr, nullptr);
        return utf8Str;
#else
        return std::filesystem::path(std::wstring(s, size)).string();
#endif
    }

    std::wstring utf8ToWide(const char *s, int size) {
#ifdef _WIN32
        if (size < 0) {
            size = (int) strlen(s);
        }
        if (size == 0) {
            return {};
        }
        const int utf16Length =
            ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, size, nullptr, 0);
        if (utf16Length <= 0) {
            return {};
        }
        std::wstring utf16Str;
        utf16Str.resize(utf16Length);
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, size, utf16Str.data(), utf16Length);
        return utf16Str;
#else
        return std::filesystem::path(std::string(s, size)).wstring();
#endif
    }

#ifdef _WIN32
    std::string ansiToUtf8(const char *s, int size) {
        if (size < 0) {
            size = (int) strlen(s);
        }
        if (size == 0) {
            return {};
        }
        const int utf16Length =
            ::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, s, size, nullptr, 0);
        if (utf16Length <= 0) {
            return {};
        }
        std::wstring utf16Str;
        utf16Str.resize(utf16Length);
        ::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, s, size, utf16Str.data(), utf16Length);
        return wideToUtf8(utf16Str.data(), int(utf16Str.size()));
    }
#endif

}