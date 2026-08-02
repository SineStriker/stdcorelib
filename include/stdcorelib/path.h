#ifndef STDCORELIB_PATH_H
#define STDCORELIB_PATH_H

#include <string>
#include <filesystem>

#include <stdcorelib/str.h>

namespace stdc {

    namespace path {

        /// Builds a path from UTF-8. std::filesystem::path takes a narrow string in the ANSI
        /// code page on Windows, which mangles anything outside it, so go through here instead.
        inline std::filesystem::path from_utf8(const std::string_view &s) {
#ifdef _WIN32
            return wstring_conv::from_utf8(s);
#else
            return s;
#endif
        }

        /// The other direction. path::string() is the lossy one on Windows, for the same reason.
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

        /// std::filesystem::canonical without the throw, returning an empty path on failure.
        /// This one touches the file system and needs the path to exist.
        inline std::filesystem::path canonical(const std::filesystem::path &path) {
            std::error_code ec;
            return std::filesystem::canonical(path, ec);
        }

        /// Resolves "." and ".." lexically, without looking at the file system, so it works on a
        /// path that does not exist. A symlink followed by ".." therefore lands somewhere
        /// canonical() would not.
        STDCORELIB_EXPORT std::filesystem::path clean_path(const std::filesystem::path &path);

        /// Rewrites the separators as '/', or as the platform's own when \a native is set.
        inline std::string normalize_separators(const std::filesystem::path &path,
                                                bool native = false) {
            return str::conv<std::filesystem::path>::normalize_separators(to_utf8(path), native);
        }

    }

    using path::clean_path;
    using path::normalize_separators;

}

#endif // STDCORELIB_PATH_H