#ifndef STDCORELIB_STR_H
#define STDCORELIB_STR_H

#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <type_traits>
#include <filesystem>
#include <map>
#include <algorithm>
#include <functional>
#include <system_error>

#include <stdcorelib/stdc_global.h>

namespace stdc {

    namespace str {

        template <class T>
        struct conv;

        template <>
        struct conv<std::string> {
            inline std::string operator()(const std::string &s) const {
                return s;
            }

            // ##FIXME: remove?
            inline std::string operator()(std::string &&s) const {
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

            static inline std::wstring from_utf8(const std::string_view &s) {
                return from_utf8(s.data(), int(s.size()));
            }

            STDCORELIB_EXPORT static std::string to_utf8(const wchar_t *s, int size = -1);

            static inline std::string to_utf8(const std::wstring_view &s) {
                return to_utf8(s.data(), int(s.size()));
            }

#ifdef _WIN32
            STDCORELIB_EXPORT static std::wstring from_ansi(const char *s, int size = -1);

            static inline std::wstring from_ansi(const std::string_view &s) {
                return from_ansi(s.data(), int(s.size()));
            }

            STDCORELIB_EXPORT static std::string to_ansi(const wchar_t *s, int size = -1);

            static inline std::string to_ansi(const std::wstring_view &s) {
                return to_ansi(s.data(), int(s.size()));
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
                return str::conv<T2>()(t);
            } else {
                using T2 = std::decay_t<std::remove_cv_t<T1>>;
                if constexpr (std::is_same_v<T2, bool>) {
                    return t ? "true" : "false";
                } else if constexpr (std::is_same_v<T2, char>) {
                    return std::string(t);
                } else if constexpr (std::is_same_v<T2, wchar_t>) {
                    return conv<std::wstring>::to_utf8(&t, 1);
                } else if constexpr (std::is_integral_v<T2>) {
                    return std::to_string(t);
                } else if constexpr (std::is_floating_point_v<T2>) {
                    std::ostringstream oss;
                    oss << std::noshowpoint << t;
                    return oss.str();
                } else {
                    return str::conv<T2>()(std::forward<T>(t));
                }
            }
        }

        STDCORELIB_EXPORT std::string join(const std::vector<std::string> &v,
                                           const std::string_view &delimiter);

        // @overload: join(vector<string_view>, string_view)
        STDCORELIB_EXPORT std::string join(const std::vector<std::string_view> &v,
                                           const std::string_view &delimiter);

        STDCORELIB_EXPORT std::vector<std::string_view> split(const std::string_view &s,
                                                              const std::string_view &delimiter);

        // @overload: split(string &&, string_view)
        STDCORELIB_EXPORT std::vector<std::string> split(std::string &&s,
                                                         const std::string_view &delimiter);

        STDCORELIB_EXPORT std::string format(const std::string_view &fmt,
                                             const std::vector<std::string> &args);

        template <class Arg1, class... Args>
        std::string formatN(const std::string_view &fmt, Arg1 &&arg1, Args &&...args) {
            return format(fmt, {
                                   to_string(std::forward<Arg1>(arg1)),
                                   to_string(std::forward<Args>(args))...,
                               });
        }

        // @overload: formatN(string_view)
        inline std::string_view formatN(const std::string_view &fmt) {
            return fmt;
        }

        // @overload: formatN(string &&)
        inline std::string formatN(std::string &&fmt) {
            return fmt;
        }

        // @overload: formatN(const char *)
        inline const char *formatN(const char *fmt) {
            return fmt;
        }

        STDCORELIB_EXPORT std::string
            varexp(const std::string_view &s,
                   const std::function<std::string(const std::string_view &)> &find);

        // @overload: varexp(string, map<string, string>)
        template <class MAP>
        inline std::string varexp(const std::string_view &s, const MAP &vars) {
            return varexp(s, [&vars](const std::string_view &name) -> std::string {
                auto it = vars.find(name);
                if (it == vars.end())
                    return std::string();
                return it->second;
            });
        }

    }

    using str::to_string;
    using str::join;
    using str::split;
    using str::format;
    using str::formatN;

    using wstring_conv = str::conv<std::wstring>;

    namespace str {

        inline std::string to_upper(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return s;
        }

        // @overload: to_upper(wstring)
        inline std::wstring to_upper(std::wstring s) {
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return s;
        }

        inline std::string to_lower(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        }

        // @overload: to_lower(wstring)
        inline std::wstring to_lower(std::wstring s) {
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        }

        inline bool starts_with(const std::string_view &s, const std::string_view &prefix) {
#if __cplusplus >= 202002L
            return s.starts_with(prefix);
#else
            return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
#endif
        }

        // @overload: starts_with(string, char)
        inline bool starts_with(const std::string_view &s, char prefix) {
            return s.size() >= 1 && s.front() == prefix;
        }

        // @overload: starts_with(wstring_view, wstring_view)
        inline bool starts_with(const std::wstring_view &s, const std::wstring_view &prefix) {
#if __cplusplus >= 202002L
            return s.starts_with(prefix);
#else
            return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
#endif
        }

        // @overload: starts_with(wstring_view, wchar_t)
        inline bool starts_with(const std::wstring_view &s, wchar_t prefix) {
            return s.size() >= 1 && s.front() == prefix;
        }

        inline bool ends_with(const std::string_view &s, const std::string_view &suffix) {
#if __cplusplus >= 202002L
            return s.ends_with(suffix);
#else
            return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
#endif
        }

        // @overload: ends_with(string_view, char)
        inline bool ends_with(const std::string_view &s, char suffix) {
            return s.size() >= 1 && s.back() == suffix;
        }

        // @overload: ends_with(wstring_view, wstring_view)
        inline bool ends_with(const std::wstring_view &s, const std::wstring_view &suffix) {
#if __cplusplus >= 202002L
            return s.ends_with(suffix);
#else
            return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
#endif
        }

        // @overload: ends_with(wstring_view, wchar_t)
        inline bool ends_with(const std::wstring_view &s, wchar_t suffix) {
            return s.size() >= 1 && s.back() == suffix;
        }

        inline std::string_view drop_front(const std::string_view &s, size_t N = 1) {
            return s.substr(N);
        }

        // @overload: drop_front(string &&, size_t)
        inline std::string drop_front(std::string &&s, size_t N = 1) {
            return s.substr(N);
        }

        inline std::string_view drop_back(const std::string_view &s, size_t N = 1) {
            return s.substr(0, s.size() - N);
        }

        // @overload: drop_back(string &&, size_t)
        inline std::string drop_back(std::string &&s, size_t N = 1) {
            return s.substr(0, s.size() - N);
        }

        inline std::string_view ltrim(const std::string_view &s, char Char) {
            return drop_front(s, std::min(s.size(), s.find_first_not_of(Char)));
        }

        // @overload: ltrim(string &&, char)
        inline std::string ltrim(std::string &&s, char Char) {
            return std::string(
                drop_front(std::string_view(s), std::min(s.size(), s.find_first_not_of(Char))));
        }

        // @overload: ltrim(string_view, string_view)
        inline std::string_view ltrim(const std::string_view &s,
                                      const std::string_view &Chars = " \t\n\v\f\r") {
            return drop_front(s, std::min(s.size(), s.find_first_not_of(Chars)));
        }

        // @overload: ltrim(string &&, string_view)
        inline std::string ltrim(std::string &&s, const std::string_view &Chars = " \t\n\v\f\r") {
            return std::string(
                drop_front(std::string_view(s), std::min(s.size(), s.find_first_not_of(Chars))));
        }

        inline std::string_view rtrim(const std::string_view &s, char Char) {
            return drop_back(s, s.size() - std::min(s.size(), s.find_last_not_of(Char) + 1));
        }

        // @overload: rtrim(string &&, char)
        inline std::string rtrim(std::string &&s, char Char) {
            return std::string(drop_back(
                std::string_view(s), s.size() - std::min(s.size(), s.find_last_not_of(Char) + 1)));
        }

        // @overload: rtrim(string_view, string_view)
        inline std::string_view rtrim(const std::string_view &s,
                                      const std::string_view &Chars = " \t\n\v\f\r") {
            return drop_back(s, s.size() - std::min(s.size(), s.find_last_not_of(Chars) + 1));
        }

        // @overload: rtrim(string &&, string)
        inline std::string rtrim(std::string &&s, const std::string_view &Chars = " \t\n\v\f\r") {
            return std::string(drop_back(
                std::string_view(s), s.size() - std::min(s.size(), s.find_last_not_of(Chars) + 1)));
        }

        inline std::string_view trim(const std::string_view &s, char Char) {
            return rtrim(ltrim(s, Char), Char);
        }

        // @overload: trim(string &&, char)
        inline std::string trim(std::string &&s, char Char) {
            return std::string(rtrim(ltrim(std::string_view(s), Char), Char));
        }

        // @overload: trim(string_view, string_view)
        inline std::string_view trim(const std::string_view &s,
                                     std::string_view Chars = " \t\n\v\f\r") {
            return rtrim(ltrim(s, Chars), Chars);
        }

        // @overload: trim(string &&, string)
        inline std::string trim(std::string &&s, std::string_view Chars = " \t\n\v\f\r") {
            return std::string(rtrim(ltrim(std::string_view(s), Chars), Chars));
        }

        inline bool contains(const std::string_view &s, const std::string_view &sub) {
            return s.find(sub) != std::string_view::npos;
        }

        // @overload: contains(string_view, char)
        inline bool contains(const std::string_view &s, char c) {
            return s.find(c) != std::string_view::npos;
        }

    }

    using str::starts_with;
    using str::ends_with;
    using str::to_lower;
    using str::to_upper;
    using str::ltrim;
    using str::rtrim;
    using str::trim;

#ifdef _WIN32
    const std::error_category &windows_utf8_category() noexcept;
#endif

}

#endif // STDCORELIB_STR_H