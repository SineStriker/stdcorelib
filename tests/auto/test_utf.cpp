// SPDX-License-Identifier: MIT

#include <initializer_list>
#include <string>

#include <stdcorelib/utf.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_utf)

namespace {

    // Spelled as bytes so nothing depends on how the compiler treats a literal.
    std::string bytes(std::initializer_list<int> v) {
        std::string s;
        for (int c : v) {
            s.push_back(char(static_cast<unsigned char>(c)));
        }
        return s;
    }

    std::u16string units(std::initializer_list<int> v) {
        std::u16string s;
        for (int c : v) {
            s.push_back(char16_t(c));
        }
        return s;
    }

    constexpr char32_t Fffd = utf::replacement_character;

}

BOOST_AUTO_TEST_CASE(test_round_trip) {
    // ASCII, two-byte, three-byte, and one past the BMP so a surrogate pair is involved
    const std::u32string source = {U'A', 0x00E9, 0x4F60, 0x1F600, U'z'};

    auto u8 = utf::utf32_to_utf8(source);
    auto u16 = utf::utf32_to_utf16(source);

    BOOST_CHECK_EQUAL(u8.size(), 1u + 2u + 3u + 4u + 1u);
    BOOST_CHECK_EQUAL(u16.size(), 1u + 1u + 1u + 2u + 1u); // the emoji takes two units

    BOOST_CHECK(utf::utf8_to_utf32(u8) == source);
    BOOST_CHECK(utf::utf16_to_utf32(u16) == source);
    BOOST_CHECK(utf::utf8_to_utf16(u8) == u16);
    BOOST_CHECK(utf::utf16_to_utf8(u16) == u8);
}

BOOST_AUTO_TEST_CASE(test_empty_input_is_not_a_failure) {
    bool ok = false;
    BOOST_CHECK(utf::utf8_to_utf16({}, utf::fail, &ok).empty());
    BOOST_CHECK(ok); // empty in, empty out, nothing went wrong

    ok = false;
    BOOST_CHECK(utf::utf16_to_utf8({}, utf::fail, &ok).empty());
    BOOST_CHECK(ok);
}

// The encodings that mean something already spelled in fewer bytes. Accepting them would let the
// same text be written more than one way, which is how a check on the short form gets bypassed.
BOOST_AUTO_TEST_CASE(test_overlong_is_rejected) {
    const std::string cases[] = {
        bytes({0xC0, 0x80}),             // NUL the long way
        bytes({0xC1, 0xBF}),             // U+007F the long way
        bytes({0xE0, 0x80, 0x80}),       // NUL the longer way
        bytes({0xE0, 0x9F, 0xBF}),       // below the three-byte range
        bytes({0xF0, 0x80, 0x80, 0x80}), // NUL longer still
        bytes({0xF0, 0x8F, 0xBF, 0xBF}), // below the four-byte range
    };
    for (const auto &s : cases) {
        BOOST_CHECK(!utf::is_valid_utf8(s));

        bool ok = true;
        BOOST_CHECK(utf::utf8_to_utf32(s, utf::fail, &ok).empty());
        BOOST_CHECK(!ok);
    }
}

// D800 to DFFF exist only to spell a pair in UTF-16 and are not characters, so UTF-8 must not
// carry them. This is the difference between UTF-8 and CESU-8.
BOOST_AUTO_TEST_CASE(test_surrogates_are_rejected) {
    BOOST_CHECK(!utf::is_valid_utf8(bytes({0xED, 0xA0, 0x80}))); // U+D800
    BOOST_CHECK(!utf::is_valid_utf8(bytes({0xED, 0xBF, 0xBF}))); // U+DFFF
    BOOST_CHECK(utf::is_valid_utf8(bytes({0xED, 0x9F, 0xBF})));  // U+D7FF, just below
    BOOST_CHECK(utf::is_valid_utf8(bytes({0xEE, 0x80, 0x80})));  // U+E000, just above

    // and neither may a UTF-32 string
    BOOST_CHECK(!utf::is_valid_utf32(std::u32string{0xD800}));
    BOOST_CHECK(!utf::is_valid_utf32(std::u32string{0xDFFF}));
    BOOST_CHECK(utf::is_valid_utf32(std::u32string{0xD7FF}));
}

BOOST_AUTO_TEST_CASE(test_out_of_range_is_rejected) {
    BOOST_CHECK(utf::is_valid_utf8(bytes({0xF4, 0x8F, 0xBF, 0xBF})));  // U+10FFFF, the last one
    BOOST_CHECK(!utf::is_valid_utf8(bytes({0xF4, 0x90, 0x80, 0x80}))); // one past it
    BOOST_CHECK(!utf::is_valid_utf8(bytes({0xF5, 0x80, 0x80, 0x80}))); // no lead byte goes here
    BOOST_CHECK(!utf::is_valid_utf8(bytes({0xFF})));

    BOOST_CHECK(utf::is_valid_utf32(std::u32string{utf::max_code_point}));
    BOOST_CHECK(!utf::is_valid_utf32(std::u32string{utf::max_code_point + 1}));
}

BOOST_AUTO_TEST_CASE(test_stray_and_truncated_bytes) {
    BOOST_CHECK(!utf::is_valid_utf8(bytes({0x80})));             // continuation with no lead
    BOOST_CHECK(!utf::is_valid_utf8(bytes({0xE4, 0xBD})));       // three-byte cut short
    BOOST_CHECK(!utf::is_valid_utf8(bytes({0xF0, 0x9F, 0x98}))); // four-byte cut short
}

// One bad byte costs one replacement character, not the rest of the string. Consuming the
// maximal part that could still have been valid is what keeps a single error local.
BOOST_AUTO_TEST_CASE(test_replacement_is_local) {
    // 'A', a stray continuation byte, then 'B'
    auto out = utf::utf8_to_utf32(bytes({'A', 0x80, 'B'}));
    BOOST_REQUIRE_EQUAL(out.size(), 3u);
    BOOST_CHECK(out[0] == U'A');
    BOOST_CHECK(out[1] == Fffd);
    BOOST_CHECK(out[2] == U'B');

    // a lead byte whose second byte is wrong, followed by text that is fine
    out = utf::utf8_to_utf32(bytes({0xE0, 0x41, 0x42}));
    BOOST_REQUIRE_EQUAL(out.size(), 3u);
    BOOST_CHECK(out[0] == Fffd);
    BOOST_CHECK(out[1] == U'A');
    BOOST_CHECK(out[2] == U'B');

    // a truncated sequence at the end is one error, not one per byte
    out = utf::utf8_to_utf32(bytes({'A', 0xF0, 0x9F, 0x98}));
    BOOST_REQUIRE_EQUAL(out.size(), 2u);
    BOOST_CHECK(out[0] == U'A');
    BOOST_CHECK(out[1] == Fffd);
}

// A conversion writes into a fixed buffer until the input outgrows it. Nothing may change at
// that threshold, and the widest code points are the ones that reach it first.
BOOST_AUTO_TEST_CASE(test_input_outgrowing_the_inline_buffer) {
    const char32_t cycle[] = {U'A', 0x00E9, 0x4F60, 0x1F600};

    for (size_t n : {size_t(1), size_t(85), size_t(86), size_t(256), size_t(257), size_t(4096)}) {
        std::u32string source;
        for (size_t i = 0; i < n; ++i) {
            source.push_back(cycle[i % 4]);
        }

        const auto u8 = utf::utf32_to_utf8(source);
        const auto u16 = utf::utf32_to_utf16(source);

        BOOST_CHECK(utf::utf8_to_utf32(u8) == source);
        BOOST_CHECK(utf::utf16_to_utf32(u16) == source);
        BOOST_CHECK(utf::utf8_to_utf16(u8) == u16);
        BOOST_CHECK(utf::utf16_to_utf8(u16) == u8);
        BOOST_CHECK(utf::wide_to_utf8(utf::utf8_to_wide(u8)) == u8);
    }
}

BOOST_AUTO_TEST_CASE(test_policies) {
    const auto bad = bytes({'A', 0x80, 'B'});

    bool ok = true;
    auto replaced = utf::utf8_to_utf16(bad, utf::replace, &ok);
    BOOST_CHECK(!ok); // it still says the input was bad
    BOOST_CHECK_EQUAL(replaced.size(), 3u);

    ok = true;
    auto failed = utf::utf8_to_utf16(bad, utf::fail, &ok);
    BOOST_CHECK(!ok);
    BOOST_CHECK(failed.empty());

    // valid input reports success under either
    ok = false;
    BOOST_CHECK_EQUAL(utf::utf8_to_utf16("hello", utf::fail, &ok).size(), 5u);
    BOOST_CHECK(ok);
}

BOOST_AUTO_TEST_CASE(test_utf16_surrogate_pairing) {
    BOOST_CHECK(utf::is_valid_utf16(units({0xD83D, 0xDE00}))); // a proper pair
    BOOST_CHECK(!utf::is_valid_utf16(units({0xD83D})));        // high with nothing after it
    BOOST_CHECK(!utf::is_valid_utf16(units({0xDE00})));        // low with nothing before it
    BOOST_CHECK(!utf::is_valid_utf16(units({0xD83D, 'A'})));   // high followed by something else

    auto out = utf::utf16_to_utf32(units({'A', 0xD83D, 'B'}));
    BOOST_REQUIRE_EQUAL(out.size(), 3u);
    BOOST_CHECK(out[1] == Fffd);
    BOOST_CHECK(out[2] == U'B'); // the letter after the stray surrogate survives
}

// wchar_t is UTF-16 on Windows and UTF-32 elsewhere, so the wide functions have to pick. Either
// way the text has to survive the round trip.
BOOST_AUTO_TEST_CASE(test_wide) {
    const std::string source = utf::utf32_to_utf8(std::u32string{U'A', 0x4F60, 0x1F600});

    auto wide = utf::utf8_to_wide(source);
    BOOST_CHECK_EQUAL(wide.size(), sizeof(wchar_t) == 2 ? 4u : 3u);
    BOOST_CHECK_EQUAL(utf::wide_to_utf8(wide), source);
}

BOOST_AUTO_TEST_CASE(test_valid_utf8_shapes) {
    BOOST_CHECK(utf::is_valid_utf8(""));
    BOOST_CHECK(utf::is_valid_utf8("plain ascii"));
    BOOST_CHECK(utf::is_valid_utf8(bytes({0x00})));             // NUL is a character
    BOOST_CHECK(utf::is_valid_utf8(bytes({0xC2, 0x80})));       // U+0080, the shortest two-byte
    BOOST_CHECK(utf::is_valid_utf8(bytes({0xDF, 0xBF})));       // U+07FF, the longest two-byte
    BOOST_CHECK(utf::is_valid_utf8(bytes({0xE0, 0xA0, 0x80}))); // U+0800
    BOOST_CHECK(utf::is_valid_utf8(bytes({0xEF, 0xBF, 0xBF}))); // U+FFFF
    BOOST_CHECK(utf::is_valid_utf8(bytes({0xF0, 0x90, 0x80, 0x80}))); // U+10000
}

BOOST_AUTO_TEST_SUITE_END()
