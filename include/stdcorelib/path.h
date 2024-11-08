#ifndef STDCORELIB_PATH_H
#define STDCORELIB_PATH_H

#include <string>
#include <filesystem>

#include <stdcorelib/strings.h>

namespace stdc {

    namespace path {

        STDCORELIB_EXPORT std::filesystem::path clean_path(const std::filesystem::path &path);

        STDCORELIB_EXPORT std::string normalize_separators(const std::string &path,
                                                           bool native = false);

        inline std::filesystem::path from_utf8(const std::string &s) {
#ifdef _WIN32
            return strings::conv<std::wstring>::from_utf8(s);
#else
            return s;
#endif
        }

        inline std::string to_utf8(const std::filesystem::path &path) {
#ifdef _WIN32
            return strings::conv<std::wstring>::to_utf8(path.wstring());
#else
            return path.string();
#endif
        }

    }

    using path::clean_path;

}

#endif // STDCORELIB_PATH_H