// SPDX-License-Identifier: MIT

#include "commandline.h"

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>

namespace stdc::cli {

    namespace detail {

        namespace {

            /// Whether \a token is entirely made of what \c from_chars consumed. A number that
            /// stops short of the end of the token, \c 12abc, is not a number.
            bool consumed_all(std::string_view token, const char *end) {
                return end == token.data() + token.size();
            }

            /// \c from_chars refuses a leading plus, which a command line will hand over anyway.
            std::string_view drop_leading_plus(std::string_view token) {
                if (token.size() > 1 && token.front() == '+') {
                    token.remove_prefix(1);
                }
                return token;
            }

            bool equals_ignoring_case(std::string_view token, std::string_view other) {
                if (token.size() != other.size()) {
                    return false;
                }
                for (size_t i = 0; i < token.size(); ++i) {
                    char a = token[i];
                    if (a >= 'A' && a <= 'Z') {
                        a = char(a - 'A' + 'a');
                    }
                    if (a != other[i]) {
                        return false;
                    }
                }
                return true;
            }

        }

        bool parse_signed(std::string_view token, int64_t *out, int64_t min, int64_t max) {
            token = drop_leading_plus(token);
            if (token.empty()) {
                return false;
            }
            int64_t v = 0;
            auto res = std::from_chars(token.data(), token.data() + token.size(), v);
            if (res.ec != std::errc{} || !consumed_all(token, res.ptr)) {
                return false;
            }
            if (v < min || v > max) {
                return false;
            }
            *out = v;
            return true;
        }

        bool parse_unsigned(std::string_view token, uint64_t *out, uint64_t max) {
            token = drop_leading_plus(token);
            if (token.empty()) {
                return false;
            }
            // No sign to refuse by hand: from_chars into an unsigned rejects a minus outright.
            // Checked on all three of MSVC, libstdc++ and libc++, each answering invalid_argument
            // and leaving the output alone. The test says so too, since it is their promise this
            // relies on rather than ours.
            uint64_t v = 0;
            auto res = std::from_chars(token.data(), token.data() + token.size(), v);
            if (res.ec != std::errc{} || !consumed_all(token, res.ptr)) {
                return false;
            }
            if (v > max) {
                return false;
            }
            *out = v;
            return true;
        }

        bool parse_floating(std::string_view token, double *out) {
            if (token.empty()) {
                return false;
            }
            // strtod rather than from_chars: libc++ went years without the floating point
            // overload, and macOS is one of the platforms this has to work on.
            std::string buf(token);
            errno = 0;
            char *end = nullptr;
            double v = std::strtod(buf.c_str(), &end);
            if (end != buf.c_str() + buf.size() || end == buf.c_str()) {
                return false;
            }
            if (errno == ERANGE) {
                return false;
            }
            *out = v;
            return true;
        }

        bool parse_boolean(std::string_view token, bool *out) {
            static const std::string_view yes[] = {"true", "yes", "on", "1"};
            static const std::string_view no[] = {"false", "no", "off", "0"};
            for (auto word : yes) {
                if (equals_ignoring_case(token, word)) {
                    *out = true;
                    return true;
                }
            }
            for (auto word : no) {
                if (equals_ignoring_case(token, word)) {
                    *out = false;
                    return true;
                }
            }
            return false;
        }

    }

}
