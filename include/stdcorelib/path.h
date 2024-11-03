#ifndef STDCORELIB_PATH_H
#define STDCORELIB_PATH_H

#include <string>
#include <filesystem>

#include <stdcorelib/codec.h>

namespace stdc {

    STDCORELIB_EXPORT std::filesystem::path cleanPath(const std::filesystem::path &path);

    STDCORELIB_EXPORT std::string normalizePathSeparators(const std::string &path,
                                                        bool native = false);

    inline std::filesystem::path utf8ToPath(const std::string &s) {
#ifdef _WIN32
        return utf8ToWide(s);
#else
        return s;
#endif
    }

    inline std::string pathToUtf8(const std::filesystem::path &path) {
#ifdef _WIN32
        return wideToUtf8(path.wstring());
#else
        return path.string();
#endif
    }

}

#endif // STDCORELIB_PATH_H