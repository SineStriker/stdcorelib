// SPDX-License-Identifier: MIT

#include <stdcorelib/console.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
// For the case that asks the console this process is attached to.
#  include <io.h>

#  include <stdcorelib/platform/windows/stdc_windows.h>
#else
// For the case that asks a real terminal, which means making one.
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <termios.h>
#  include <unistd.h>
#endif

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
    // verbatim, markup and all. Note this differs from str::varexp(), which returns an empty
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

    // More live targets than the cache has slots. Every one still resolves correctly. The ones
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
    using console::detail::attributes;
    using console::detail::sgr_sequence;

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
    using console::detail::attributes;
    using console::detail::sgr_reset_sequence;

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
    using console::detail::attributes;

    BOOST_CHECK(attributes{} == attributes({nostyle, nocolor, nocolor}));
    BOOST_CHECK(attributes({bold, red, blue}) == attributes({bold, red, blue}));
    BOOST_CHECK(attributes({bold, red, blue}) != attributes({bold, red, cyan}));
    BOOST_CHECK(attributes({bold, red, blue}) != attributes({italic, red, blue}));
    BOOST_CHECK(attributes({bold, red, blue}) != attributes({bold, green, blue}));
}

// How much room a piece of text takes on a terminal, which is not how long it is in bytes and
// not how long it is in characters either.
BOOST_AUTO_TEST_CASE(test_display_width_counts_columns) {
    BOOST_CHECK_EQUAL(display_width(""), 0);
    BOOST_CHECK_EQUAL(display_width("hello"), 5);

    // Two columns each, three bytes each.
    BOOST_CHECK_EQUAL(display_width("\xe4\xb8\xad\xe6\x96\x87"), 4); // CJK
    BOOST_CHECK_EQUAL(display_width("\xef\xbc\xa1"), 2);             // fullwidth A
    BOOST_CHECK_EQUAL(display_width("\xf0\x9f\x98\x80"), 2);         // an emoji, four bytes

    // Mixed, so the sum is not a multiple of anything.
    BOOST_CHECK_EQUAL(display_width("a\xe4\xb8\xad"
                                    "b"),
                      4);

    // A combining mark hangs on the character before it and takes no room of its own.
    BOOST_CHECK_EQUAL(display_width("e\xcc\x81"), 1);

    // Per code point, for callers walking text rather than measuring it whole. Spelled as
    // escapes so the answers do not rest on how a compiler read this file.
    BOOST_CHECK_EQUAL(display_width(U'a'), 1);
    BOOST_CHECK_EQUAL(display_width(U'中'), 2); // a CJK ideograph
    BOOST_CHECK_EQUAL(display_width(U'́'), 0); // a combining acute
    BOOST_CHECK_EQUAL(display_width(U'\U0001F600'), 2);
}

// A file is not a terminal, so there is nothing to ask and the fallback is the answer.
BOOST_AUTO_TEST_CASE(test_width_falls_back_off_a_terminal) {
    TempFile file;
    BOOST_REQUIRE(file.get() != nullptr);

    // COLUMNS would win if it were set, and this case is about what happens without it.
    const char *saved = std::getenv("COLUMNS");
    std::string keep = saved ? saved : std::string();
#ifdef _WIN32
    _putenv_s("COLUMNS", "");
#else
    unsetenv("COLUMNS");
#endif

    BOOST_CHECK_EQUAL(width(file.get()), 80);
    BOOST_CHECK_EQUAL(width(file.get(), 42), 42);

    // COLUMNS is the override every terminal-aware program honors, and the only say a caller
    // has when the target is not a terminal at all.
#ifdef _WIN32
    _putenv_s("COLUMNS", "123");
#else
    setenv("COLUMNS", "123", 1);
#endif
    BOOST_CHECK_EQUAL(width(file.get()), 123);
    BOOST_CHECK_EQUAL(width(file.get(), 42), 123);

    // Something that is not a positive number is not an answer, so the fallback stands.
    for (const char *bad : {"", "0", "-5", "wide"}) {
#ifdef _WIN32
        _putenv_s("COLUMNS", bad);
#else
        setenv("COLUMNS", bad, 1);
#endif
        BOOST_CHECK_MESSAGE(width(file.get(), 42) == 42, std::string("COLUMNS=") + bad);
    }

#ifdef _WIN32
    _putenv_s("COLUMNS", keep.c_str());
#else
    if (saved) {
        setenv("COLUMNS", keep.c_str(), 1);
    } else {
        unsetenv("COLUMNS");
    }
#endif
}

#ifdef _WIN32

// The console this process is attached to, asked directly. Its own stdout is very often a pipe,
// which is what CONOUT$ is for: it names the console rather than whatever the standard handles
// were redirected to.
//
// Read only. Setting the size would be resizing the window somebody is watching the suite in.
BOOST_AUTO_TEST_CASE(test_width_asks_a_real_console) {
    FILE *console = nullptr;
    // Read as well as write. GetConsoleScreenBufferInfo wants GENERIC_READ on the handle, and
    // a write only "w" does not carry it, which comes back as the fallback and looks like the
    // branch was taken and answered 80.
    fopen_s(&console, "CONOUT$", "r+");
    if (!console) {
        // No console attached, which is how a service or some CI runners start a process. There
        // is nothing to ask, and inventing one means a pseudoconsole and a second process.
        BOOST_TEST_MESSAGE("no console attached, so the console branch is not exercised here");
        return;
    }

    const char *saved = std::getenv("COLUMNS");
    std::string keep = saved ? saved : std::string();
    _putenv_s("COLUMNS", "");

    // Whatever it says, it has to be the console's own answer rather than the fallback. Two
    // different fallbacks, so a branch that ignored the console entirely cannot pass both.
    int first = width(console, 4242);
    int second = width(console, 909);
    BOOST_CHECK_GT(first, 0);
    BOOST_CHECK_EQUAL(first, second);
    BOOST_CHECK_MESSAGE(first != 4242 && first != 909,
                        "the console branch fell back rather than answering");

    // And it is the visible window rather than the scrollback buffer.
    //
    // This does not discriminate on every console. Windows Terminal gives the buffer the same
    // width as the window, so measuring dwSize.X instead passes here and was measured doing so.
    // It would fail on a console where somebody widened the buffer, which is the case the code
    // is written for.
    CONSOLE_SCREEN_BUFFER_INFO info{};
    HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(console)));
    BOOST_REQUIRE(::GetConsoleScreenBufferInfo(handle, &info));
    BOOST_CHECK_EQUAL(first, int(info.srWindow.Right) - int(info.srWindow.Left) + 1);

    if (saved) {
        _putenv_s("COLUMNS", keep.c_str());
    }
    std::fclose(console);
}

#endif // _WIN32

#ifndef _WIN32

// What a real terminal says, which is the whole point of the function and the one thing a file
// cannot stand in for. A pty is a terminal, and its size is ours to set.
BOOST_AUTO_TEST_CASE(test_width_asks_a_real_terminal) {
    int master = ::posix_openpt(O_RDWR | O_NOCTTY);
    BOOST_REQUIRE(master >= 0);
    BOOST_REQUIRE_EQUAL(::grantpt(master), 0);
    BOOST_REQUIRE_EQUAL(::unlockpt(master), 0);
    const char *name = ::ptsname(master);
    BOOST_REQUIRE(name != nullptr);
    int slave = ::open(name, O_RDWR | O_NOCTTY);
    BOOST_REQUIRE(slave >= 0);
    FILE *file = ::fdopen(slave, "w");
    BOOST_REQUIRE(file != nullptr);

    // COLUMNS would win over the terminal, and this case is about the terminal.
    const char *saved = std::getenv("COLUMNS");
    std::string keep = saved ? saved : std::string();
    ::unsetenv("COLUMNS");

    const auto &resize = [&](unsigned short columns) {
        struct winsize ws {};
        ws.ws_col = columns;
        ws.ws_row = 40;
        BOOST_REQUIRE_EQUAL(::ioctl(slave, TIOCSWINSZ, &ws), 0);
    };

    resize(137);
    BOOST_CHECK_EQUAL(width(file), 137);
    // The fallback is not consulted when there is a real answer.
    BOOST_CHECK_EQUAL(width(file, 42), 137);

    // Asked afresh every call, so a terminal resized under a running program is followed.
    resize(37);
    BOOST_CHECK_EQUAL(width(file), 37);

    // A terminal that answers zero has told us nothing, which happens under some multiplexers.
    resize(0);
    BOOST_CHECK_EQUAL(width(file), 80);
    BOOST_CHECK_EQUAL(width(file, 42), 42);

    if (saved) {
        ::setenv("COLUMNS", keep.c_str(), 1);
    }
    std::fclose(file);
    ::close(master);
}

#endif // !_WIN32

BOOST_AUTO_TEST_SUITE_END()
