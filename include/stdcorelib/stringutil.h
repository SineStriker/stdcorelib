#ifndef STDCORELIB_STRINGUTIL_H
#define STDCORELIB_STRINGUTIL_H

#include <string_view>
#include <sstream>

#include <stdcorelib/path.h>
#include <stdcorelib/codec.h>

namespace stdc {

    template <class T>
    struct StringConverter;

    template <class T>
    std::string any2str(T &&t) {
        using T2 = typename std::decay_t<std::remove_cv_t<std::remove_reference_t<T>>>;
        if constexpr (std::is_same_v<T2, bool>) {
            return t ? "true" : "false";
        } else if constexpr (std::is_same_v<T2, char>) {
            return std::string(t);
        } else if constexpr (std::is_integral_v<T2>) {
            return std::to_string(t);
        } else if constexpr (std::is_floating_point_v<T2>) {
            std::ostringstream oss;
            oss << std::noshowpoint << t;
            return oss.str();
        } else {
            return StringConverter<T2>()(t);
        }
    }

    template <>
    struct StringConverter<std::filesystem::path> {
        std::string operator()(const std::filesystem::path &path) {
            return normalizePathSeparators(path2u8str(path), true);
        }
    };

    template <>
    struct StringConverter<std::string> {
        std::string operator()(const std::string &s) {
            return s;
        }
    };

    template <>
    struct StringConverter<std::wstring> {
        std::string operator()(const std::wstring &s) {
            return wideToUtf8(s);
        }
    };

    template <>
    struct StringConverter<wchar_t *> {
        std::string operator()(const wchar_t *s) {
            return wideToUtf8(s);
        }
    };

    template <>
    struct StringConverter<const wchar_t *> {
        std::string operator()(const wchar_t *s) {
            return wideToUtf8(s);
        }
    };

    template <>
    struct StringConverter<char *> {
        std::string operator()(const char *s) {
            return s;
        }
    };

    template <>
    struct StringConverter<const char *> {
        std::string operator()(const char *s) {
            return s;
        }
    };

    STDCORELIB_EXPORT std::vector<std::string_view> split(const std::string_view &s,
                                                        const std::string_view &delimiter);

    STDCORELIB_EXPORT std::string join(const std::vector<std::string> &v,
                                     const std::string_view &delimiter);

}

#endif // STDCORELIB_STRINGUTIL_H