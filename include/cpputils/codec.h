#ifndef CPPUTILS_CODEC_H
#define CPPUTILS_CODEC_H

#include <string>

#include <cpputils/global.h>

namespace cpputils {

    CPPUTILS_EXPORT std::string wideToUtf8(const wchar_t *s, int size = -1);

    inline std::string wideToUtf8(const std::wstring &s) {
        return wideToUtf8(s.c_str(), int(s.size()));
    }

    CPPUTILS_EXPORT std::wstring utf8ToWide(const char *s, int size = -1);

    inline std::wstring utf8ToWide(const std::string &s) {
        return utf8ToWide(s.c_str(), int(s.size()));
    }

#ifdef _WIN32
    CPPUTILS_EXPORT std::string ansiToUtf8(const char *s, int size = -1);

    inline std::string ansiToUtf8(const std::string &s) {
        return ansiToUtf8(s.c_str(), int(s.size()));
    }
#endif

}

#endif // CPPUTILS_CODEC_H
