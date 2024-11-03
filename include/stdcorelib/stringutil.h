#ifndef STDCORELIB_STRINGUTIL_H
#define STDCORELIB_STRINGUTIL_H

#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <type_traits>

#include <stdcorelib/path.h>
#include <stdcorelib/codec.h>

namespace stdc {

    template <class T>
    struct string_conv;

    template <class T>
    std::string to_string(T &&t) {
        using T1 = std::remove_reference_t<T>;
        if constexpr (std::is_pointer_v<T1>) {
            using T2 =
                std::add_pointer_t<std::decay_t<std::remove_cv_t<std::remove_pointer_t<T1>>>>;
            return string_conv<T2>()(t);
        } else {
            using T2 = std::decay_t<std::remove_cv_t<T1>>;
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
                return string_conv<T2>()(t);
            }
        }
    }

    template <>
    struct string_conv<std::string> {
        std::string operator()(const std::string &s) {
            return s;
        }
    };

    template <>
    struct string_conv<char *> {
        std::string operator()(const char *s) {
            return s;
        }
    };

    template <>
    struct string_conv<std::wstring> {
        std::string operator()(const std::wstring &s) {
            return wideToUtf8(s);
        }
    };

    template <>
    struct string_conv<wchar_t *> {
        std::string operator()(const wchar_t *s) {
            return wideToUtf8(s);
        }
    };

    template <>
    struct string_conv<std::filesystem::path> {
        std::string operator()(const std::filesystem::path &path) {
            return normalizePathSeparators(pathToUtf8(path), true);
        }
    };

    STDCORELIB_EXPORT std::vector<std::string_view> split(const std::string_view &s,
                                                          const std::string_view &delimiter);

    STDCORELIB_EXPORT std::string join(const std::vector<std::string> &v,
                                       const std::string_view &delimiter);

}

#endif // STDCORELIB_STRINGUTIL_H