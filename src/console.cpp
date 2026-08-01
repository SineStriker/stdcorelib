#include "console.h"

#ifdef _WIN32
#  include "stdc_windows.h"
#  include <io.h>
#else
#  include <unistd.h>
#endif

#include <atomic>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "console_p.h"
#include "str_p.h"

namespace stdc {

    // Everything below asks about the *target* being written to, never about the process's own
    // stdout, so redirected output is not styled just because stdout happens to be a terminal.

    // What the target turned out to be. Independent of the configured color mode, so it can be
    // remembered across writes.
    enum target_kind {
        target_not_a_terminal,
        target_vt,
        target_legacy, // a Windows console that would not take virtual terminal processing
    };

#ifdef _WIN32
    using target_id = HANDLE;
    static const target_id invalid_target_id = INVALID_HANDLE_VALUE;
#else
    using target_id = int;
    static const target_id invalid_target_id = -1;
#endif

    // The descriptor behind a file, without asking the OS anything about it. No system call, so
    // it is cheap enough to check on every write -- which is what lets a remembered answer be
    // matched against the file it was actually probed through.
    static target_id target_id_of(FILE *file) {
        if (!file) {
            return invalid_target_id;
        }
#ifdef _WIN32
        int fd = ::_fileno(file);
        if (fd < 0) {
            return invalid_target_id;
        }
        return reinterpret_cast<HANDLE>(::_get_osfhandle(fd));
#else
        int fd = ::fileno(file);
        return fd < 0 ? invalid_target_id : fd;
#endif
    }

    // Asks the OS what the target is, and on Windows switches virtual terminal processing on
    // while it is there. This is the part that costs system calls and, on a console, changes
    // state -- so it is done once per target rather than once per write.
    static target_kind detect_target(FILE *file) {
        target_id id = target_id_of(file);
        if (id == invalid_target_id) {
            return target_not_a_terminal;
        }
#ifdef _WIN32
        DWORD mode = 0;
        if (!::GetConsoleMode(id, &mode)) {
            return target_not_a_terminal;
        }
        if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
            return target_vt;
        }
        return ::SetConsoleMode(id, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) ? target_vt
                                                                               : target_legacy;
#else
        return ::isatty(id) ? target_vt : target_not_a_terminal;
#endif
    }

    // A handful of slots is plenty: in practice this only ever sees stdout and stderr. A target
    // that does not fit is simply probed every time, which is correct, just not free.
    //
    // An entry is trusted only while the file still resolves to the descriptor it was probed
    // through. That check costs nothing and catches a FILE being closed and its address reused.
    // It does not catch freopen() swapping the target underneath the same descriptor, which is
    // what set_color_mode() clearing the table is for.
    struct target_cache_entry {
        FILE *file;
        target_id id;
        target_kind kind;
    };

    static constexpr int target_cache_slots = 4;
    static target_cache_entry g_target_cache[target_cache_slots]{};
    static int g_target_cache_size = 0;

    static std::mutex &target_cache_mtx() {
        static std::mutex instance;
        return instance;
    }

    static void clear_target_cache() {
        std::lock_guard<std::mutex> lock(target_cache_mtx());
        g_target_cache_size = 0;
    }

    static target_kind target_kind_of(FILE *file) {
        target_id id = target_id_of(file);
        if (id == invalid_target_id) {
            return target_not_a_terminal;
        }

        std::lock_guard<std::mutex> lock(target_cache_mtx());

        int slot = -1;
        for (int i = 0; i < g_target_cache_size; ++i) {
            if (g_target_cache[i].file == file) {
                slot = i;
                break;
            }
        }
        if (slot >= 0 && g_target_cache[slot].id == id) {
            return g_target_cache[slot].kind;
        }

        target_kind kind = detect_target(file);
        if (slot < 0 && g_target_cache_size < target_cache_slots) {
            slot = g_target_cache_size++;
        }
        if (slot >= 0) {
            g_target_cache[slot] = {file, id, kind};
        }
        return kind;
    }

    class ConsoleOutputGuard {
    public:
        class BaseOutput {
        public:
            BaseOutput() = default;
            virtual ~BaseOutput() = default;

            void enter(FILE *file) {
                global_mtx().lock();
                _file = file;
#ifdef _WIN32
                // Switching the code page is a process wide side effect, so only pay it when the
                // target really is a console; for a file or a pipe it would change nothing there
                // and disturb whatever else is writing to the console. Both calls below are
                // answered from the cache, so this costs no system call of its own.
                _console = target_kind_of(file) != target_not_a_terminal ? target_id_of(file)
                                                                         : INVALID_HANDLE_VALUE;
                if (_console != INVALID_HANDLE_VALUE) {
                    _codepage = ::GetConsoleOutputCP();
                    ::SetConsoleOutputCP(CP_UTF8);
                }
#endif
                attach();
            }

            void leave() {
                reset();
#ifdef _WIN32
                if (_console != INVALID_HANDLE_VALUE) {
                    ::SetConsoleOutputCP(_codepage);
                    _console = INVALID_HANDLE_VALUE;
                }
#endif
                _file = nullptr;
                global_mtx().unlock();
            }

            virtual void change(int style, int fg, int bg) = 0;
            virtual void reset() = 0;

        protected:
            // Runs once the target is in place, for whatever a backend has to read off it.
            virtual void attach() {
            }

            FILE *_file = nullptr;

            static const int _style_init = console::nostyle;
            static const int _fg_init = console::nocolor;
            static const int _bg_init = console::nocolor;

            int _style = _style_init;
            int _fg = _fg_init;
            int _bg = _bg_init;

            console::detail::attributes current() const {
                return {_style, _fg, _bg};
            }

            void set_current(const console::detail::attributes &attrs) {
                _style = attrs.style;
                _fg = attrs.fg;
                _bg = attrs.bg;
            }

            static std::mutex &global_mtx() {
                static std::mutex _instance;
                return _instance;
            }

#ifdef _WIN32
            HANDLE _console = INVALID_HANDLE_VALUE;

        private:
            UINT _codepage{};
#endif
        };

        // Writes no attributes at all. Used whenever the target cannot show them, which is the
        // case for every file and pipe -- and is what makes the text path deterministic enough
        // to unit test.
        class PlainOutput : public BaseOutput {
        public:
            void change(int style, int fg, int bg) override {
                (void) style;
                (void) fg;
                (void) bg;
            }
            void reset() override {
            }
        };

        class VTOutput : public BaseOutput {
        public:
            VTOutput() = default;
            void change(int style, int fg, int bg) override {
                console::detail::attributes next{style, fg, bg};
                auto seq = console::detail::sgr_sequence(current(), next);
                if (!seq.empty()) {
                    std::ignore = std::fputs(seq.c_str(), _file);
                }
                // Assigned even when nothing was emitted, so an attribute that has no escape code
                // of its own still counts as the state we are now in.
                set_current(next);
            }

            void reset() override {
                auto seq = console::detail::sgr_reset_sequence(current());
                if (!seq.empty()) {
                    std::ignore = std::fputs(seq.c_str(), _file);
                    set_current({});
                }
            }
        };

#ifdef _WIN32
        class WindowsLegacyOutput : public BaseOutput {
        public:
            WindowsLegacyOutput() = default;

        protected:
            // The attributes to restore are read off the target that was just attached, not off
            // the process's stdout.
            void attach() override {
                if (_console != INVALID_HANDLE_VALUE) {
                    ::GetConsoleScreenBufferInfo(_console, &_csbi);
                }
            }

        public:
            void change(int style, int fg, int bg) override {
                (void) style;

                WORD winColor = 0;
                bool needSet = false;
                if (fg != _fg) {
                    _fg = fg;
                    needSet = true;

                    if (fg != console::nocolor) {
                        winColor |= (fg & console::intensified) ? FOREGROUND_INTENSITY : 0;
                        switch (fg & 0xF) {
                            case console::red:
                                winColor |= FOREGROUND_RED;
                                break;
                            case console::green:
                                winColor |= FOREGROUND_GREEN;
                                break;
                            case console::blue:
                                winColor |= FOREGROUND_BLUE;
                                break;
                            case console::yellow:
                                winColor |= FOREGROUND_RED | FOREGROUND_GREEN;
                                break;
                            case console::purple:
                                winColor |= FOREGROUND_RED | FOREGROUND_BLUE;
                                break;
                            case console::cyan:
                                winColor |= FOREGROUND_GREEN | FOREGROUND_BLUE;
                                break;
                            case console::white:
                                winColor |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
                            default:
                                break;
                        }

                        // emulate bold effect
                        if (style & console::bold) {
                            winColor |= FOREGROUND_INTENSITY;
                        }
                    }
                }

                if (bg != _bg) {
                    _bg = bg;
                    needSet = true;

                    if (bg != console::nocolor) {
                        winColor |= (bg & console::intensified) ? BACKGROUND_INTENSITY : 0;
                        switch (bg & 0xF) {
                            case console::red:
                                winColor |= BACKGROUND_RED;
                                break;
                            case console::green:
                                winColor |= BACKGROUND_GREEN;
                                break;
                            case console::blue:
                                winColor |= BACKGROUND_BLUE;
                                break;
                            case console::yellow:
                                winColor |= BACKGROUND_RED | BACKGROUND_GREEN;
                                break;
                            case console::purple:
                                winColor |= BACKGROUND_RED | BACKGROUND_BLUE;
                                break;
                            case console::cyan:
                                winColor |= BACKGROUND_GREEN | BACKGROUND_BLUE;
                                break;
                            case console::white:
                                winColor |= BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
                            default:
                                break;
                        }
                    }
                }

                if (needSet && _console != INVALID_HANDLE_VALUE) {
                    ::SetConsoleTextAttribute(_console, winColor);
                }
            }

            void reset() override {
                if (_fg != _fg_init || _bg != _bg_init) {
                    if (_console != INVALID_HANDLE_VALUE) {
                        ::SetConsoleTextAttribute(_console, _csbi.wAttributes);
                    }

                    _fg = _fg_init;
                    _bg = _bg_init;
                }
            }

        private:
            CONSOLE_SCREEN_BUFFER_INFO _csbi{};
        };
#endif

        static BaseOutput &output_for(FILE *file) {
            switch (console::resolve_color_mode(file)) {
                case console::color_mode::vt: {
                    static VTOutput instance;
                    return instance;
                }
#ifdef _WIN32
                case console::color_mode::windows_legacy: {
                    static WindowsLegacyOutput instance;
                    return instance;
                }
#endif
                default:
                    break;
            }
            static PlainOutput instance;
            return instance;
        }

        ConsoleOutputGuard(FILE *file) : _output(&output_for(file)) {
            _output->enter(file);
        }

        ~ConsoleOutputGuard() {
            _output->leave();
        }

        void change(int style, int fg, int bg) {
            _output->change(style, fg, bg);
        }

        void reset() {
            _output->reset();
        }

    private:
        BaseOutput *_output;
    };

    /*！
        \namespace console
        \brief Namespace of console related functions.
    */

    namespace console {

        namespace detail {

            // The SGR code for a foreground color, or nullptr when there is none. Both `nocolor`
            // and `black` land in the default branch: neither has ever had a code here, and
            // giving them one now would change how existing markup renders.
            static const char *fg_code(int fg) {
                const bool light = fg & intensified;
                switch (fg & 0xF) {
                    case red:
                        return light ? "91" : "31";
                    case green:
                        return light ? "92" : "32";
                    case blue:
                        return light ? "94" : "34";
                    case yellow:
                        return light ? "93" : "33";
                    case purple:
                        return light ? "95" : "35";
                    case cyan:
                        return light ? "96" : "36";
                    case white:
                        return light ? "97" : "37";
                    default:
                        return nullptr;
                }
            }

            static const char *bg_code(int bg) {
                const bool light = bg & intensified;
                switch (bg & 0xF) {
                    case red:
                        return light ? "101" : "41";
                    case green:
                        return light ? "102" : "42";
                    case blue:
                        return light ? "104" : "44";
                    case yellow:
                        return light ? "103" : "43";
                    case purple:
                        return light ? "105" : "45";
                    case cyan:
                        return light ? "106" : "46";
                    case white:
                        return light ? "107" : "47";
                    default:
                        return nullptr;
                }
            }

            std::string sgr_sequence(const attributes &from, const attributes &to) {
                std::string codes;
                const auto &push = [&codes](const char *code) {
                    if (!codes.empty()) {
                        codes += ';';
                    }
                    codes += code;
                };

                if (to.fg != from.fg) {
                    if (const char *code = fg_code(to.fg)) {
                        push(code);
                    }
                }
                if (to.bg != from.bg) {
                    if (const char *code = bg_code(to.bg)) {
                        push(code);
                    }
                }
                if (to.style != from.style) {
                    if (to.style & bold) {
                        push("1");
                    }
                    if (to.style & italic) {
                        push("3");
                    }
                    if (to.style & underline) {
                        push("4");
                    }
                    if (to.style & strikethrough) {
                        push("9");
                    }
                }

                if (codes.empty()) {
                    return {};
                }
                return "\033[" + codes + "m";
            }

            std::string sgr_reset_sequence(const attributes &from) {
                if (from == attributes{}) {
                    return {};
                }
                return "\033[0m";
            }

        }

        static std::atomic<color_mode> g_color_mode{color_mode::automatic};

        /*!
            Returns the color mode the process is set to.
        */
        color_mode get_color_mode() {
            return g_color_mode.load(std::memory_order_relaxed);
        }

        /*!
            Overrides how styling is emitted, process wide.
        */
        void set_color_mode(color_mode mode) {
            g_color_mode.store(mode, std::memory_order_relaxed);
            // Also the way to make a target be probed again, which a program that has swapped one
            // out with freopen() needs.
            clear_target_cache();
        }

        /*!
            Returns the color mode that will actually be used for \a file.
        */
        color_mode resolve_color_mode(FILE *file) {
            auto mode = g_color_mode.load(std::memory_order_relaxed);
            if (mode != color_mode::automatic) {
#ifndef _WIN32
                // No console API to drive here. Writing nothing beats writing escape sequences
                // the caller did not ask for.
                if (mode == color_mode::windows_legacy) {
                    return color_mode::never;
                }
#endif
                return mode;
            }

            // Probing the target is the expensive half, and on a console it also turns virtual
            // terminal processing on, so it happens once per target and is remembered.
            switch (target_kind_of(file)) {
                case target_vt:
                    return color_mode::vt;
#ifdef _WIN32
                case target_legacy:
                    return color_mode::windows_legacy;
#endif
                default:
                    break;
            }
            return color_mode::never;
        }

        int fputs(int style, int fg, int bg, const char *buf, FILE *file) {
            ConsoleOutputGuard cog(file);
            cog.change(style, fg, bg);
            return std::fputs(buf, file);
        }

        int fputs(int style, int fg, int bg, const std::string_view &buf, FILE *file) {
            ConsoleOutputGuard cog(file);
            cog.change(style, fg, bg);
            return std::fwrite(buf.data(), sizeof(char), buf.size(), file);
        }

        int puts(int style, int fg, int bg, const char *buf) {
            int ret = console::fputs(style, fg, bg, buf, stdout);
            // flush the internal buffer after resetting the console attributes
            if (std::putchar('\n') != EOF) {
                ret += 1;
            }
            return ret;
        }

        int puts(int style, int fg, int bg, const std::string_view &buf) {
            int ret = console::fputs(style, fg, bg, buf, stdout);
            // flush the internal buffer after resetting the console attributes
            if (std::putchar('\n') != EOF) {
                ret += 1;
            }
            return ret;
        }

        /*!
            Print formatted string in UTF-8 encoding with specified styles to file.
        */
        int fprintf(int style, int fg, int bg, FILE *file, const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            int ret = console::vfprintf(style, fg, bg, file, fmt, args);
            va_end(args);
            return ret;
        }

        /*!
            Print formatted string in UTF-8 encoding with specified styles to file.
        */
        int vfprintf(int style, int fg, int bg, FILE *file, const char *fmt, va_list args) {
            ConsoleOutputGuard cog(file);
            cog.change(style, fg, bg);
            return std::vfprintf(file, fmt, args);
        }

        /*!
            Print formatted string in UTF-8 encoding with specified styles.
        */
        int printf(int style, int fg, int bg, const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            int ret = console::vprintf(style, fg, bg, fmt, args);
            va_end(args);
            return ret;
        }

        /*!
            Print formatted string in UTF-8 encoding with specified styles.
        */
        int vprintf(int style, int fg, int bg, const char *fmt, va_list args) {
            return console::vfprintf(style, fg, bg, stdout, fmt, args);
        }

        /*!
            \fn void print(int foreground, int background, const std::string &format, Args
           &&...args)

            Print formatted string in UTF-8 encoding with specified styles.
        */

        /*!
            \fn void println(int foreground, int background, const std::string &format, Args
           &&...args)

            Print formatted string in UTF-8 encoding with specified styles and start a new line.
        */

        int u8fprintf(FILE *file, const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            int ret = console::u8vfprintf(file, fmt, args);
            va_end(args);
            return ret;
        }

        int u8vfprintf(FILE *file, const char *fmt, va_list args) {
            return console::vfprintf(nostyle, nocolor, nocolor, file, fmt, args);
        }

        /*!
            Print formatted string in UTF-8 encoding with default styles.
        */
        int u8printf(const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            int ret = console::u8vprintf(fmt, args);
            va_end(args);
            return ret;
        }

        /*!
            Print formatted string in UTF-8 encoding with default styles.
        */
        int u8vprintf(const char *fmt, va_list args) {
            return console::vfprintf(nostyle, nocolor, nocolor, stdout, fmt, args);
        }

        static int cfputs_get_color_name(const std::string_view &var) {
            if (var == "red") {
                return red;
            }
            if (var == "green") {
                return green;
            }
            if (var == "blue") {
                return blue;
            }
            if (var == "yellow") {
                return yellow;
            }
            if (var == "purple") {
                return purple;
            }
            if (var == "cyan") {
                return cyan;
            }
            if (var == "white") {
                return white;
            }
            if (var == "black") {
                return black;
            }
            if (var == "nocolor") {
                return nocolor;
            }
            return -1;
        }

        static inline int cfputs_get_color(const std::string_view &var) {
            if (starts_with(var, "light")) {
                int color = cfputs_get_color_name(var.substr(5));
                if (color == -1 || color == nocolor) {
                    return -1;
                }
                return color | intensified;
            }
            return cfputs_get_color_name(var);
        }

        // @return:
        //   reset:  false
        //   change: true
        static bool cfputs_update_attrs(const std::string_view var, int &style, int &fg, int &bg) {
            // reset?
            if (var == "reset" || var == "clear") {
                style = nostyle;
                fg = nocolor;
                bg = nocolor;
                return false;
            }

            // bg?
            if (STDCORELIB_UNLIKELY(var.front() == '@')) {
                auto var1 = var.substr(1);
                if (STDCORELIB_UNLIKELY(var1 == "intensified")) {
                    if (bg != nocolor)
                        bg |= intensified;
                    return true;
                }
                int color = cfputs_get_color(var1);
                if (color != -1) {
                    bg = color;
                }
                return true;
            }

            // fg?
            {
                if (STDCORELIB_UNLIKELY(var == "intensified")) {
                    if (fg != nocolor)
                        fg |= intensified;
                    return true;
                }
                int color = cfputs_get_color(var);
                if (color != -1) {
                    fg = color;
                    return true;
                }

                // fallthrough
            }

            // style?
            if (var == "bold") {
                style |= bold;
                return true;
            }
            if (var == "italic") {
                style |= italic;
                return true;
            }
            if (var == "nostyle") {
                style = nostyle;
                return true;
            }
            if (var == "underline") {
                style |= underline;
                return true;
            }
            if (var == "strikethrough") {
                style |= strikethrough;
                return true;
            }
            return true;
        }

        int cfputs(const char *buf, FILE *file) {
            return cfputs(std::string_view(buf), file);
        }

        int cfputs(const std::string_view &buf, FILE *file) {
            using namespace str;

            vlarray<varexp_part, 10> parts;
            if (!varexp_split(buf, parts)) {
                return std::fwrite(buf.data(), sizeof(char), buf.size(), file);
            }

            int style = nostyle;
            int fg = nocolor;
            int bg = nocolor;

            ConsoleOutputGuard cog(file);
            int ret = 0;
            for (const auto &part : parts) {
                switch (part.type) {
                    case varexp_part_type::literal: {
                        ret += std::fwrite(part.data, sizeof(char), part.size, file);
                        break;
                    }
                    case varexp_part_type::literal_with_dollar: {
                        const char *p = part.data;
                        const char *q = p + part.size;
                        const char *start = p;
                        while (p < q) {
                            if (*p == '$' && p + 1 < q && *(p + 1) == '$') {
                                ret += std::fwrite(start, sizeof(char), p - start + 1, file);
                                p += 2;
                                start = p;
                                continue;
                            }
                            p++;
                        }
                        if (start < p) {
                            ret += std::fwrite(start, sizeof(char), p - start, file);
                        }
                        break;
                    }
                    case varexp_part_type::variable: {
                        std::string_view part_view(part.data, part.size);

                        constexpr std::string_view whitespace = " \t";
                        size_t start = 0;
                        while (start < part_view.size()) {
                            // skip head whitespace
                            size_t word_start = part_view.find_first_not_of(whitespace, start);
                            if (word_start == std::string_view::npos)
                                break;
                            size_t word_end = part_view.find_first_of(whitespace, word_start);

                            // extract word
                            if (word_end == std::string_view::npos) {
                                std::string_view var = part_view.substr(word_start);

                                bool change = cfputs_update_attrs(var, style, fg, bg);
                                cog.reset();
                                if (change)
                                    cog.change(style, fg, bg);
                                break;
                            } else {
                                std::string_view var =
                                    part_view.substr(word_start, word_end - word_start);
                                bool change = cfputs_update_attrs(var, style, fg, bg);
                                cog.reset();
                                if (change)
                                    cog.change(style, fg, bg);
                            }

                            // continue scanning from the end of the current word
                            start = word_end;
                        }
                        break;
                    }
                    case varexp_part_type::nested_variable:
                        break; // invalid
                }
            }
            return ret;
        }

        int cputs(const char *buf) {
            return cputs(std::string_view(buf));
        }

        int cputs(const std::string_view &buf) {
            int ret = cfputs(buf, stdout);
            if (std::putchar('\n') != EOF) {
                ret += 1;
            }
            return ret;
        }

        int cfprintf(FILE *file, const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            int ret = cvfprintf(file, fmt, args);
            va_end(args);
            return ret;
        }

        int cvfprintf(FILE *file, const char *fmt, va_list args) {
            return cfputs(vasprintf(fmt, args), file);
        }

        int cprintf(const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            int ret = cvprintf(fmt, args);
            va_end(args);
            return ret;
        }

        STDCORELIB_EXPORT int cvprintf(const char *fmt, va_list args) {
            return cvfprintf(stdout, fmt, args);
        }

    }

}
