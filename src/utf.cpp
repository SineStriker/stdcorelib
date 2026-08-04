// SPDX-License-Identifier: MIT

#include "utf.h"

#include <cstdint>

namespace stdc::utf {

    namespace {

        // How many bytes a lead byte announces, or 0 if it cannot start a sequence. C0 and C1
        // are missing on purpose: the only things they could encode are already spelled in one
        // byte, and accepting the longer form lets the same text be written two ways. F5 and
        // above would run past U+10FFFF.
        int sequence_length(uint8_t lead) {
            if (lead < 0x80) {
                return 1;
            }
            if (lead >= 0xC2 && lead <= 0xDF) {
                return 2;
            }
            if (lead >= 0xE0 && lead <= 0xEF) {
                return 3;
            }
            if (lead >= 0xF0 && lead <= 0xF4) {
                return 4;
            }
            return 0;
        }

        bool is_continuation(uint8_t c) {
            return (c & 0xC0) == 0x80;
        }

        // The second byte carries the constraint that the lead byte alone cannot express: which
        // of the values in its range would be an overlong encoding, a surrogate, or past the end
        // of Unicode.
        bool second_byte_ok(uint8_t lead, uint8_t second) {
            switch (lead) {
                case 0xE0:
                    return second >= 0xA0 && second <= 0xBF; // shorter form exists below A0
                case 0xED:
                    return second >= 0x80 && second <= 0x9F; // above 9F is a surrogate
                case 0xF0:
                    return second >= 0x90 && second <= 0xBF; // shorter form exists below 90
                case 0xF4:
                    return second >= 0x80 && second <= 0x8F; // above 8F is past U+10FFFF
                default:
                    return is_continuation(second);
            }
        }

        /// Reads one code point starting at \a pos.
        ///
        /// On success \a pos moves past it and the code point comes back. On invalid input \a pos
        /// moves past the maximal part that could still have been the start of something valid,
        /// which is at least one byte, and the result is nothing. Consuming exactly that much is
        /// what stops one bad byte turning the rest of the text into replacement characters.
        bool decode_one(std::string_view s, size_t &pos, char32_t &out) {
            const uint8_t lead = uint8_t(s[pos]);
            const int length = sequence_length(lead);
            if (length == 0) {
                pos += 1;
                return false;
            }
            if (length == 1) {
                out = lead;
                pos += 1;
                return true;
            }

            // A sequence cut off by the end of the input is not valid, but it is not garbage
            // either, so it is consumed whole rather than one byte at a time.
            if (pos + size_t(length) > s.size()) {
                size_t taken = 1;
                while (pos + taken < s.size() && is_continuation(uint8_t(s[pos + taken]))) {
                    ++taken;
                }
                pos += taken;
                return false;
            }

            if (!second_byte_ok(lead, uint8_t(s[pos + 1]))) {
                pos += 1;
                return false;
            }
            for (int i = 2; i < length; ++i) {
                if (!is_continuation(uint8_t(s[pos + i]))) {
                    pos += size_t(i);
                    return false;
                }
            }

            char32_t c = lead & (0xFF >> (length + 1));
            for (int i = 1; i < length; ++i) {
                c = (c << 6) | (uint8_t(s[pos + i]) & 0x3F);
            }
            pos += size_t(length);
            out = c;
            return true;
        }

        void encode_utf8(char32_t c, std::string &out) {
            if (c < 0x80) {
                out.push_back(char(c));
            } else if (c < 0x800) {
                out.push_back(char(0xC0 | (c >> 6)));
                out.push_back(char(0x80 | (c & 0x3F)));
            } else if (c < 0x10000) {
                out.push_back(char(0xE0 | (c >> 12)));
                out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
                out.push_back(char(0x80 | (c & 0x3F)));
            } else {
                out.push_back(char(0xF0 | (c >> 18)));
                out.push_back(char(0x80 | ((c >> 12) & 0x3F)));
                out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
                out.push_back(char(0x80 | (c & 0x3F)));
            }
        }

        void encode_utf16(char32_t c, std::u16string &out) {
            if (c < 0x10000) {
                out.push_back(char16_t(c));
            } else {
                c -= 0x10000;
                out.push_back(char16_t(0xD800 + (c >> 10)));
                out.push_back(char16_t(0xDC00 + (c & 0x3FF)));
            }
        }

        /// Reads one code point of UTF-16, following the same contract as decode_one().
        bool decode_one_utf16(std::u16string_view s, size_t &pos, char32_t &out) {
            const char16_t unit = s[pos];
            if (unit < 0xD800 || unit > 0xDFFF) {
                out = unit;
                pos += 1;
                return true;
            }
            if (unit >= 0xDC00) { // a low surrogate with no high one ahead of it
                pos += 1;
                return false;
            }
            if (pos + 1 >= s.size() || s[pos + 1] < 0xDC00 || s[pos + 1] > 0xDFFF) {
                pos += 1;
                return false;
            }
            out = 0x10000 + ((char32_t(unit) - 0xD800) << 10) + (char32_t(s[pos + 1]) - 0xDC00);
            pos += 2;
            return true;
        }

        // Every conversion is the same walk, so they share one. Decode is told how to read a
        // code point out of the source, Encode how to put one into the result.
        template <class Source, class Result, class Decode, class Encode>
        Result convert(Source s, error_policy policy, bool *ok, Decode decode, Encode encode) {
            Result out;
            out.reserve(s.size());

            bool valid = true;
            size_t pos = 0;
            while (pos < s.size()) {
                char32_t c = 0;
                if (decode(s, pos, c)) {
                    encode(c, out);
                    continue;
                }
                valid = false;
                if (policy == fail) {
                    if (ok) {
                        *ok = false;
                    }
                    return Result();
                }
                encode(replacement_character, out);
            }

            if (ok) {
                *ok = valid;
            }
            return out;
        }

        // UTF-32 has no encoding to get wrong, only values that are not code points.
        bool decode_one_utf32(std::u32string_view s, size_t &pos, char32_t &out) {
            const char32_t c = s[pos];
            pos += 1;
            if (!is_valid_code_point(c)) {
                return false;
            }
            out = c;
            return true;
        }

        void encode_utf32(char32_t c, std::u32string &out) {
            out.push_back(c);
        }

    }

    std::u16string utf8_to_utf16(std::string_view s, error_policy policy, bool *ok) {
        return convert<std::string_view, std::u16string>(s, policy, ok, decode_one, encode_utf16);
    }

    std::u32string utf8_to_utf32(std::string_view s, error_policy policy, bool *ok) {
        return convert<std::string_view, std::u32string>(s, policy, ok, decode_one, encode_utf32);
    }

    std::string utf16_to_utf8(std::u16string_view s, error_policy policy, bool *ok) {
        return convert<std::u16string_view, std::string>(s, policy, ok, decode_one_utf16,
                                                         encode_utf8);
    }

    std::u32string utf16_to_utf32(std::u16string_view s, error_policy policy, bool *ok) {
        return convert<std::u16string_view, std::u32string>(s, policy, ok, decode_one_utf16,
                                                            encode_utf32);
    }

    std::string utf32_to_utf8(std::u32string_view s, error_policy policy, bool *ok) {
        return convert<std::u32string_view, std::string>(s, policy, ok, decode_one_utf32,
                                                         encode_utf8);
    }

    std::u16string utf32_to_utf16(std::u32string_view s, error_policy policy, bool *ok) {
        return convert<std::u32string_view, std::u16string>(s, policy, ok, decode_one_utf32,
                                                            encode_utf16);
    }

    std::wstring utf8_to_wide(std::string_view s, error_policy policy, bool *ok) {
        static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4,
                      "wchar_t is neither UTF-16 nor UTF-32 wide");
        if constexpr (sizeof(wchar_t) == 2) {
            auto u16 = utf8_to_utf16(s, policy, ok);
            return std::wstring(u16.begin(), u16.end());
        } else {
            auto u32 = utf8_to_utf32(s, policy, ok);
            return std::wstring(u32.begin(), u32.end());
        }
    }

    std::string wide_to_utf8(std::wstring_view s, error_policy policy, bool *ok) {
        if constexpr (sizeof(wchar_t) == 2) {
            std::u16string u16(s.begin(), s.end());
            return utf16_to_utf8(u16, policy, ok);
        } else {
            std::u32string u32(s.begin(), s.end());
            return utf32_to_utf8(u32, policy, ok);
        }
    }

    bool is_valid_utf8(std::string_view s) {
        size_t pos = 0;
        while (pos < s.size()) {
            char32_t c = 0;
            if (!decode_one(s, pos, c)) {
                return false;
            }
        }
        return true;
    }

    bool is_valid_utf16(std::u16string_view s) {
        size_t pos = 0;
        while (pos < s.size()) {
            char32_t c = 0;
            if (!decode_one_utf16(s, pos, c)) {
                return false;
            }
        }
        return true;
    }

    bool is_valid_utf32(std::u32string_view s) {
        for (char32_t c : s) {
            if (!is_valid_code_point(c)) {
                return false;
            }
        }
        return true;
    }

}
