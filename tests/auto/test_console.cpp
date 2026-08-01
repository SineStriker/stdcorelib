#include <stdcorelib/console.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

// Private header: the escape sequence builder is not part of the public API, but it is the only
// piece of the color path a test can reach without a real terminal.
#include "console_p.h"

using namespace stdc;
using namespace stdc::console;

BOOST_AUTO_TEST_SUITE(test_console)

namespace {

    // A scratch file to write into and read back. The tests assert on exact bytes, which only
    // works because a file is never a terminal: under color_mode::automatic the console code
    // resolves it to `never` and writes plain text.
    class TempFile {
    public:
        TempFile() {
            _path = std::tmpnam(_buf);
#ifdef _WIN32
            fopen_s(&_file, _path.c_str(), "wb");
#else
            _file = std::fopen(_path.c_str(), "wb");
#endif
        }
        ~TempFile() {
            close();
            std::remove(_path.c_str());
        }

        FILE *get() const {
            return _file;
        }

        // Flushes, then returns everything written so far.
        std::string contents() {
            close();
            std::string res;
            FILE *in = nullptr;
#ifdef _WIN32
            fopen_s(&in, _path.c_str(), "rb");
#else
            in = std::fopen(_path.c_str(), "rb");
#endif
            if (!in) {
                return res;
            }
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) {
                res.append(buf, n);
            }
            std::fclose(in);
            return res;
        }

    private:
        void close() {
            if (_file) {
                std::fclose(_file);
                _file = nullptr;
            }
        }

        char _buf[L_tmpnam]{};
        std::string _path;
        FILE *_file = nullptr;
    };

    // Restores the process color mode on the way out, so one case cannot leak into the next.
    class ColorModeGuard {
    public:
        explicit ColorModeGuard(color_mode mode) : _saved(get_color_mode()) {
            set_color_mode(mode);
        }
        ~ColorModeGuard() {
            set_color_mode(_saved);
        }

    private:
        color_mode _saved;
    };

    std::string escaped(const std::string &s) {
        std::string res;
        for (unsigned char c : s) {
            if (c == 0x1B) {
                res += "<ESC>";
            } else if (c < 0x20) {
                res += "<" + std::to_string(int(c)) + ">";
            } else {
                res += char(c);
            }
        }
        return res;
    }

}

// A file is not a terminal, so nothing may be styled and the bytes must be exactly the text.
// This is the property that used to depend on what the process's stdout happened to be.
BOOST_AUTO_TEST_CASE(test_no_styling_to_a_file) {
    BOOST_CHECK(resolve_color_mode(stdin) != color_mode::automatic);

    TempFile f;
    BOOST_REQUIRE(f.get() != nullptr);
    BOOST_CHECK(resolve_color_mode(f.get()) == color_mode::never);

    console::fputs(nostyle, red, lightwhite, "hello", f.get());
    console::fputs(bold, green, nocolor, std::string_view(" world"), f.get());

    auto out = f.contents();
    BOOST_CHECK_EQUAL(escaped(out), "hello world");
}

BOOST_AUTO_TEST_CASE(test_plain_apis) {
    TempFile f;
    BOOST_REQUIRE(f.get() != nullptr);

    console::u8fputs("abc", f.get());
    console::u8fputs(std::string_view("de"), f.get());
    console::u8fprintf(f.get(), "-%d-%s", 42, "x");

    BOOST_CHECK_EQUAL(f.contents(), "abcde-42-x");
}

BOOST_AUTO_TEST_CASE(test_utf8_passes_through) {
    TempFile f;
    BOOST_REQUIRE(f.get() != nullptr);

    // "中文" spelled out in bytes, so the source encoding cannot affect the test
    const std::string utf8 = "\xE4\xB8\xAD\xE6\x96\x87";
    console::u8fputs(utf8, f.get());

    BOOST_CHECK_EQUAL(f.contents(), utf8);
}

// cfputs() parses ${...} markup. On a file the attributes go nowhere, so what is left must be
// exactly the surrounding text with every marker removed.
BOOST_AUTO_TEST_CASE(test_markup_is_stripped) {
    const auto &render = [](const char *input) {
        TempFile f;
        console::cfputs(input, f.get());
        return f.contents();
    };

    BOOST_CHECK_EQUAL(render("${red}hello"), "hello");
    BOOST_CHECK_EQUAL(render("a${red}b${green}c"), "abc");
    BOOST_CHECK_EQUAL(render("${red}${green}${blue}"), "");
    BOOST_CHECK_EQUAL(render("no markup at all"), "no markup at all");
    BOOST_CHECK_EQUAL(render(""), "");

    // several attributes in one marker, whitespace separated
    BOOST_CHECK_EQUAL(render("${bold red @blue}text"), "text");

    // background markers and the reset words
    BOOST_CHECK_EQUAL(render("${@blue}bg${reset}plain"), "bgplain");
    BOOST_CHECK_EQUAL(render("${clear}x"), "x");

    // an unknown word is consumed like any other marker
    BOOST_CHECK_EQUAL(render("${nosuchcolor}text"), "text");
}

// Two consecutive dollars stand for one, which is the literal_with_dollar path.
BOOST_AUTO_TEST_CASE(test_dollar_escaping) {
    const auto &render = [](const char *input) {
        TempFile f;
        console::cfputs(input, f.get());
        return f.contents();
    };

    BOOST_CHECK_EQUAL(render("$"), "$");
    BOOST_CHECK_EQUAL(render("$$"), "$");
    BOOST_CHECK_EQUAL(render("$$$"), "$$");
    BOOST_CHECK_EQUAL(render("$$$$"), "$$");
    BOOST_CHECK_EQUAL(render("a $ b $$ c"), "a $ b $ c");
    BOOST_CHECK_EQUAL(render("$$notavariable"), "$notavariable");

    // a dollar that is not followed by a brace is literal
    BOOST_CHECK_EQUAL(render("100$ and 50%"), "100$ and 50%");

    // An unbalanced brace fails the parse, and cfputs then falls back to writing the buffer
    // verbatim -- markup and all. Note this differs from str::varexp(), which returns an empty
    // string in the same situation.
    BOOST_CHECK_EQUAL(render("${red"), "${red");
    BOOST_CHECK_EQUAL(render("a ${ b"), "a ${ b");
}

BOOST_AUTO_TEST_CASE(test_color_mode_override) {
    TempFile f;
    BOOST_REQUIRE(f.get() != nullptr);

    // never wins even over a terminal
    {
        ColorModeGuard guard(color_mode::never);
        BOOST_CHECK(get_color_mode() == color_mode::never);
        BOOST_CHECK(resolve_color_mode(f.get()) == color_mode::never);
        BOOST_CHECK(resolve_color_mode(stdout) == color_mode::never);
    }

    // vt wins even over a plain file, which is what lets the escape path be tested at all
    {
        ColorModeGuard guard(color_mode::vt);
        BOOST_CHECK(resolve_color_mode(f.get()) == color_mode::vt);
    }

    // the guard put it back
    BOOST_CHECK(get_color_mode() == color_mode::automatic);
    BOOST_CHECK(resolve_color_mode(f.get()) == color_mode::never);

    // resolve never hands back `automatic` itself
    BOOST_CHECK(resolve_color_mode(f.get()) != color_mode::automatic);
    BOOST_CHECK(resolve_color_mode(stdout) != color_mode::automatic);
    BOOST_CHECK(resolve_color_mode(nullptr) == color_mode::never);

#ifndef _WIN32
    // there is no console API off Windows, so that mode degrades to writing nothing
    {
        ColorModeGuard guard(color_mode::windows_legacy);
        BOOST_CHECK(resolve_color_mode(f.get()) == color_mode::never);
    }
#endif
}

// What a target is gets probed once and remembered, so these check the remembering cannot go
// stale or wrong.
BOOST_AUTO_TEST_CASE(test_target_detection_is_cached) {
    TempFile f;
    BOOST_REQUIRE(f.get() != nullptr);

    // repeated questions about the same target agree
    auto first = resolve_color_mode(f.get());
    for (int i = 0; i < 5; ++i) {
        BOOST_CHECK(resolve_color_mode(f.get()) == first);
    }
    BOOST_CHECK(first == color_mode::never);

    // stdout and stderr are the targets the cache actually exists for
    auto out = resolve_color_mode(stdout);
    auto err = resolve_color_mode(stderr);
    BOOST_CHECK(resolve_color_mode(stdout) == out);
    BOOST_CHECK(resolve_color_mode(stderr) == err);

    // setting a mode drops what was remembered, and the answers still come back right
    set_color_mode(get_color_mode());
    BOOST_CHECK(resolve_color_mode(f.get()) == first);
    BOOST_CHECK(resolve_color_mode(stdout) == out);

    // More live targets than the cache has slots. Every one still resolves correctly; the ones
    // that do not fit are simply probed each time.
    {
        std::vector<std::unique_ptr<TempFile>> files;
        for (int i = 0; i < 8; ++i) {
            files.push_back(std::make_unique<TempFile>());
            BOOST_REQUIRE(files.back()->get() != nullptr);
        }
        for (const auto &file : files) {
            BOOST_CHECK(resolve_color_mode(file->get()) == color_mode::never);
        }
    }

    // Churn through short-lived files, which the C runtime is free to hand back at the same
    // address. A cached entry must not be trusted for one of those.
    for (int i = 0; i < 16; ++i) {
        TempFile scratch;
        BOOST_REQUIRE(scratch.get() != nullptr);
        BOOST_CHECK(resolve_color_mode(scratch.get()) == color_mode::never);
        console::u8fputs("x", scratch.get());
        BOOST_CHECK_EQUAL(scratch.contents(), "x");
    }
}

// With vt forced, the bytes written to a file are the real escape sequences.
BOOST_AUTO_TEST_CASE(test_forced_vt_emits_escapes) {
    ColorModeGuard guard(color_mode::vt);

    {
        TempFile f;
        console::fputs(nostyle, red, nocolor, "hi", f.get());
        // set red, write, then reset on the way out of the guard
        BOOST_CHECK_EQUAL(escaped(f.contents()), "<ESC>[31mhi<ESC>[0m");
    }

    {
        TempFile f;
        console::cfputs("${green}go", f.get());
        BOOST_CHECK_EQUAL(escaped(f.contents()), "<ESC>[32mgo<ESC>[0m");
    }

    // no color and no style means nothing to emit
    {
        TempFile f;
        console::fputs(nostyle, nocolor, nocolor, "bare", f.get());
        BOOST_CHECK_EQUAL(escaped(f.contents()), "bare");
    }
}

// The escape builder itself, exhaustively. This is the part that cannot be reached through a
// FILE at all without a terminal.
BOOST_AUTO_TEST_CASE(test_sgr_sequence) {
    using detail::attributes;
    using detail::sgr_sequence;

    const attributes none;

    // nothing to do
    BOOST_CHECK_EQUAL(sgr_sequence(none, none), "");
    BOOST_CHECK_EQUAL(sgr_sequence({bold, red, blue}, {bold, red, blue}), "");

    // foreground, normal and intensified
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, red, nocolor}), "\033[31m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, green, nocolor}), "\033[32m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, blue, nocolor}), "\033[34m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, yellow, nocolor}), "\033[33m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, purple, nocolor}), "\033[35m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, cyan, nocolor}), "\033[36m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, white, nocolor}), "\033[37m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, lightred, nocolor}), "\033[91m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, lightgreen, nocolor}), "\033[92m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, lightblue, nocolor}), "\033[94m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, lightyellow, nocolor}), "\033[93m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, lightpurple, nocolor}), "\033[95m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, lightcyan, nocolor}), "\033[96m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, lightwhite, nocolor}), "\033[97m");

    // background
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, nocolor, red}), "\033[41m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, nocolor, white}), "\033[47m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, nocolor, lightred}), "\033[101m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, nocolor, lightwhite}), "\033[107m");

    // styles, singly and combined, in the order fg, bg, bold, italic, underline, strikethrough
    BOOST_CHECK_EQUAL(sgr_sequence(none, {bold, nocolor, nocolor}), "\033[1m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {italic, nocolor, nocolor}), "\033[3m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {underline, nocolor, nocolor}), "\033[4m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {strikethrough, nocolor, nocolor}), "\033[9m");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {bold | italic, nocolor, nocolor}), "\033[1;3m");
    BOOST_CHECK_EQUAL(
        sgr_sequence(none, {bold | italic | underline | strikethrough, nocolor, nocolor}),
        "\033[1;3;4;9m");

    // everything at once, semicolon joined in that same order
    BOOST_CHECK_EQUAL(sgr_sequence(none, {bold, lightgreen, blue}), "\033[92;44;1m");

    // only what actually differs is emitted
    BOOST_CHECK_EQUAL(sgr_sequence({nostyle, red, nocolor}, {nostyle, red, blue}), "\033[44m");
    BOOST_CHECK_EQUAL(sgr_sequence({nostyle, red, blue}, {bold, red, blue}), "\033[1m");

    // there is no code for switching an attribute off, which is why callers reset first
    BOOST_CHECK_EQUAL(sgr_sequence({nostyle, red, nocolor}, none), "");
    BOOST_CHECK_EQUAL(sgr_sequence({bold, nocolor, nocolor}, none), "");

    // black has no code, matching what the markup has always rendered
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, black, nocolor}), "");
    BOOST_CHECK_EQUAL(sgr_sequence(none, {nostyle, nocolor, black}), "");
}

BOOST_AUTO_TEST_CASE(test_sgr_reset_sequence) {
    using detail::attributes;
    using detail::sgr_reset_sequence;

    // already at the defaults
    BOOST_CHECK_EQUAL(sgr_reset_sequence(attributes{}), "");
    BOOST_CHECK_EQUAL(sgr_reset_sequence({nostyle, nocolor, nocolor}), "");

    // any attribute at all needs the reset
    BOOST_CHECK_EQUAL(sgr_reset_sequence({nostyle, red, nocolor}), "\033[0m");
    BOOST_CHECK_EQUAL(sgr_reset_sequence({nostyle, nocolor, blue}), "\033[0m");
    BOOST_CHECK_EQUAL(sgr_reset_sequence({bold, nocolor, nocolor}), "\033[0m");
    BOOST_CHECK_EQUAL(sgr_reset_sequence({bold, red, blue}), "\033[0m");

    // even one whose color has no code of its own
    BOOST_CHECK_EQUAL(sgr_reset_sequence({nostyle, black, nocolor}), "\033[0m");
}

BOOST_AUTO_TEST_CASE(test_attributes_compare) {
    using detail::attributes;

    BOOST_CHECK(attributes{} == attributes({nostyle, nocolor, nocolor}));
    BOOST_CHECK(attributes({bold, red, blue}) == attributes({bold, red, blue}));
    BOOST_CHECK(attributes({bold, red, blue}) != attributes({bold, red, cyan}));
    BOOST_CHECK(attributes({bold, red, blue}) != attributes({italic, red, blue}));
    BOOST_CHECK(attributes({bold, red, blue}) != attributes({bold, green, blue}));
}

BOOST_AUTO_TEST_SUITE_END()
