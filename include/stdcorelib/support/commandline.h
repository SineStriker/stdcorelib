// SPDX-License-Identifier: MIT

#ifndef STDCORELIB_COMMANDLINE_H
#define STDCORELIB_COMMANDLINE_H

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <stdcorelib/stdc_global.h>
#include <stdcorelib/adt/array_view.h>

namespace stdc::cli {

    /// How a token is turned into a \c T, and what to call \c T in the help text.
    ///
    /// A command line is text, so everything here is stored as text and converted when it is
    /// read. Specialize this to accept a type of your own:
    ///
    /// \code
    ///   template <>
    ///   struct stdc::cli::value_traits<fs::path> {
    ///       static bool parse(std::string_view token, fs::path *out) {
    ///           *out = token;
    ///           return true;
    ///       }
    ///       static const char *type_name() {
    ///           return "path";
    ///       }
    ///   };
    /// \endcode
    ///
    /// \c parse returns false for a token the type cannot represent, which is what turns
    /// \c --count=x into a diagnostic rather than a zero.
    template <class T, class Enable = void>
    struct value_traits;

    namespace detail {

        /// The two halves of a type, kept as function pointers so that Argument can carry a type
        /// without being a template and without a virtual call per token.
        struct value_type_info {
            /// Whether the token is a \c T. Null means anything goes, which is the default.
            bool (*check)(std::string_view) = nullptr;
            /// What to call it when a diagnostic or the help text has to name it.
            const char *name = nullptr;
        };

        template <class T>
        bool check_value(std::string_view token) {
            T out{};
            return value_traits<T>::parse(token, &out);
        }

        template <class T>
        value_type_info type_info_for() {
            return {&check_value<T>, value_traits<T>::type_name()};
        }

        STDC_EXPORT bool parse_signed(std::string_view token, int64_t *out, int64_t min,
                                      int64_t max);
        STDC_EXPORT bool parse_unsigned(std::string_view token, uint64_t *out, uint64_t max);
        STDC_EXPORT bool parse_floating(std::string_view token, double *out);
        STDC_EXPORT bool parse_boolean(std::string_view token, bool *out);

    }

    /// Text, which is what a command line already is.
    template <>
    struct value_traits<std::string> {
        static inline bool parse(std::string_view token, std::string *out) {
            out->assign(token);
            return true;
        }
        static inline const char *type_name() {
            return "string";
        }
    };

    /// A view into the result's own storage, which outlives the read.
    template <>
    struct value_traits<std::string_view> {
        static inline bool parse(std::string_view token, std::string_view *out) {
            *out = token;
            return true;
        }
        static inline const char *type_name() {
            return "string";
        }
    };

    /// \c true, \c false, \c yes, \c no, \c on, \c off, \c 1 and \c 0, in any case.
    template <>
    struct value_traits<bool> {
        static inline bool parse(std::string_view token, bool *out) {
            return detail::parse_boolean(token, out);
        }
        static inline const char *type_name() {
            return "bool";
        }
    };

    /// Every integer type but \c bool, which is spelled out above. The range of the target type is
    /// part of the check, so \c 300 is not a \c uint8_t.
    template <class T>
    struct value_traits<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
        static inline bool parse(std::string_view token, T *out) {
            if constexpr (std::is_signed_v<T>) {
                int64_t v;
                if (!detail::parse_signed(token, &v, int64_t((std::numeric_limits<T>::min)()),
                                          int64_t((std::numeric_limits<T>::max)()))) {
                    return false;
                }
                *out = T(v);
            } else {
                uint64_t v;
                if (!detail::parse_unsigned(token, &v, uint64_t((std::numeric_limits<T>::max)()))) {
                    return false;
                }
                *out = T(v);
            }
            return true;
        }
        static inline const char *type_name() {
            return std::is_signed_v<T> ? "int" : "uint";
        }
    };

    /// \c float, \c double and \c long double. Uses \c strtod rather than \c from_chars, which
    /// libc++ did not implement for floating point for a long time.
    template <class T>
    struct value_traits<T, std::enable_if_t<std::is_floating_point_v<T>>> {
        static inline bool parse(std::string_view token, T *out) {
            double v;
            if (!detail::parse_floating(token, &v)) {
                return false;
            }
            *out = T(v);
            return true;
        }
        static inline const char *type_name() {
            return "number";
        }
    };

}

#endif // STDCORELIB_COMMANDLINE_H
