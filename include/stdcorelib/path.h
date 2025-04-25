#ifndef STDCORELIB_PATH_H
#define STDCORELIB_PATH_H

#include <string>
#include <filesystem>

#include <stdcorelib/str.h>

namespace stdc {

    namespace path {

        inline std::filesystem::path from_utf8(const std::string_view &s) {
#ifdef _WIN32
            return wstring_conv::from_utf8(s);
#else
            return s;
#endif
        }

        inline std::string to_utf8(const std::filesystem::path &path) {
#ifdef _WIN32
            return wstring_conv::to_utf8(path.wstring());
#else
            return path.string();
#endif
        }

        inline std::string to_utf8(const std::filesystem::path::string_type &path) {
#ifdef _WIN32
            return wstring_conv::to_utf8(path);
#else
            return path;
#endif
        }

        inline std::filesystem::path canonical(const std::filesystem::path &path) {
            try {
                return std::filesystem::canonical(path);
            } catch (...) {
            }
            return {};
        }

        STDCORELIB_EXPORT std::filesystem::path clean_path(const std::filesystem::path &path);

        inline std::string normalize_separators(const std::filesystem::path &path,
                                                bool native = false) {
            return str::conv<std::filesystem::path>::normalize_separators(to_utf8(path),
                                                                              native);
        }

    }

    using path::clean_path;

}

#endif // STDCORELIB_PATH_H