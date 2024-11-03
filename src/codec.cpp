#include "codec.h"

#ifdef _WIN32
#  include "windows_utils.h"
#else
#  include <filesystem>
#endif

namespace stdc {

    /*!
        Returns the UTF-8 encoded string converted from wide string.
    */
    std::string wideToUtf8(const wchar_t *s, int size) {
#ifdef _WIN32
        return winWideTo8bit(std::wstring_view(s, size), CP_UTF8, MB_ERR_INVALID_CHARS);
#else
        return std::filesystem::path(std::wstring(s, size)).string();
#endif
    }

    /*!
        Returns the wide string converted from UTF-8 encoded string.
    */
    std::wstring utf8ToWide(const char *s, int size) {
#ifdef _WIN32
        return win8bitToWide(std::string_view(s, size), CP_UTF8, MB_ERR_INVALID_CHARS);
#else
        return std::filesystem::path(std::string(s, size)).wstring();
#endif
    }

#ifdef _WIN32
    /*!
        Returns the local 8 bit string converted from UTF-8 encoded string, Windows only.
    */
    std::string ansiToUtf8(const char *s, int size) {
        return wideToUtf8(win8bitToWide(std::string_view(s, size), CP_ACP, MB_ERR_INVALID_CHARS));
    }
#endif

}