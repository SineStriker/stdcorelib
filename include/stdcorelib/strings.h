#ifndef STDCORELIB_STRINGS_H
#define STDCORELIB_STRINGS_H

#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <type_traits>
#include <filesystem>
#include <map>

#include <stdcorelib/global.h>

namespace stdc {

    namespace strings {

        template <class T>
        struct conv;

        template <>
        struct conv<std::string> {
            inline std::string operator()(const std::string &s) const {
                return s;
            }
        };

        template <>
        struct conv<std::string_view> {
            inline std::string operator()(const std::string_view &s) const {
                return {s.data(), s.size()};
            }
        };

        template <>
        struct conv<char *> {
            inline std::string operator()(const char *s) const {
                return s;
            }
        };

        template <>
        struct conv<std::wstring> {
            inline std::string operator()(const std::wstring &s) const {
                return to_utf8(s);
            }

            STDCORELIB_EXPORT static std::wstring from_utf8(const char *s, int size = -1);

            static inline std::wstring from_utf8(const std::string &s) {
                return from_utf8(s.c_str(), int(s.size()));
            }

            STDCORELIB_EXPORT static std::string to_utf8(const wchar_t *s, int size = -1);

            static inline std::string to_utf8(const std::wstring &s) {
                return to_utf8(s.c_str(), int(s.size()));
            }

#ifdef _WIN32
            STDCORELIB_EXPORT static std::wstring from_ansi(const char *s, int size = -1);

            static inline std::wstring from_ansi(const std::string &s) {
                return from_ansi(s.c_str(), int(s.size()));
            }

            STDCORELIB_EXPORT static std::string to_ansi(const wchar_t *s, int size = -1);

            static inline std::string to_ansi(const std::wstring &s) {
                return to_ansi(s.c_str(), int(s.size()));
            }
#endif
        };

        template <>
        struct conv<std::wstring_view> {
            inline std::string operator()(const std::wstring_view &s) const {
                return conv<std::wstring>::to_utf8(s.data(), int(s.size()));
            }
        };

        template <>
        struct conv<wchar_t *> {
            inline std::string operator()(const wchar_t *s) const {
                return conv<std::wstring>::to_utf8(s);
            }
        };

        template <>
        struct conv<std::filesystem::path> {
            inline std::string operator()(const std::filesystem::path &path) const {
#ifdef _WIN32
                return normalize_separators(conv<std::wstring>::to_utf8(path.wstring()), true);
#else
                return normalize_separators(path.string(), true);
#endif
            }

            STDCORELIB_EXPORT static std::string normalize_separators(const std::string &utf8_path,
                                                                      bool native);
        };

        template <class T>
        std::string to_string(T &&t) {
            using T1 = std::remove_reference_t<T>;
            if constexpr (std::is_pointer_v<T1>) {
                using T2 =
                    std::add_pointer_t<std::decay_t<std::remove_cv_t<std::remove_pointer_t<T1>>>>;
                return strings::conv<T2>()(t);
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
                    return strings::conv<T2>()(t);
                }
            }
        }

        STDCORELIB_EXPORT std::string join(const std::vector<std::string> &v,
                                           const std::string_view &delimiter);

        STDCORELIB_EXPORT std::vector<std::string_view> split(const std::string_view &s,
                                                              const std::string_view &delimiter);

        STDCORELIB_EXPORT std::string format(const std::string_view &fmt,
                                             const std::vector<std::string> &args);

        template <class... Args>
        auto formatN(const std::string_view &fmt, Args &&...args) {
            return format(fmt, {to_string(std::forward<decltype(args)>(args))...});
        }

        STDCORELIB_EXPORT std::string parse_expr(const std::string_view &s,
                                                 const std::map<std::string, std::string> &vars);

    }

    using strings::format;
    using strings::formatN;
    using strings::join;
    using strings::split;
    using strings::to_string;

    using wstring_conv = strings::conv<std::wstring>;

}

#endif // STDCORELIB_STRINGS_H