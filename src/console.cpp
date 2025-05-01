#include "console.h"

#ifdef _WIN32
#  include "stdc_windows.h"
#endif

#include <mutex>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "str_p.h"

namespace stdc {

    class ConsoleOutputGuard {
    public:
        class BaseOutput {
        public:
            BaseOutput() = default;
            virtual ~BaseOutput() = default;

            void enter(FILE *file) {
                global_mtx().lock();
#ifdef _WIN32
                _codepage = ::GetConsoleOutputCP();
                ::SetConsoleOutputCP(CP_UTF8);
#endif
                _file = file;
            }

            void leave() {
                reset();
#ifdef _WIN32
                ::SetConsoleOutputCP(_codepage);
#endif
                global_mtx().unlock();
            }

            virtual void change(int style, int fg, int bg) = 0;
            virtual void reset() = 0;

        protected:
            FILE *_file = nullptr;

            static const int _style_init = console::nostyle;
            static const int _fg_init = console::nocolor;
            static const int _bg_init = console::nocolor;

            int _style = _style_init;
            int _fg = _fg_init;
            int _bg = _bg_init;

            static std::mutex &global_mtx() {
                static std::mutex _instance;
                return _instance;
            }

#ifdef _WIN32
        private:
            UINT _codepage{};
#endif
        };

        class VTOutput : public BaseOutput {
        public:
            VTOutput() = default;
            void change(int style, int fg, int bg) override {
                const char *strList[10];
                int strListSize = 0;
                auto str_push = [&](const char *s) {
                    strList[strListSize] = s;
                    strListSize++;
                };
                if (fg != _fg) {
                    _fg = fg;

                    bool light = fg & console::intensified;
                    const char *colorStr = nullptr;
                    switch (fg & 0xF) {
                        case console::red:
                            colorStr = light ? "91" : "31";
                            break;
                        case console::green:
                            colorStr = light ? "92" : "32";
                            break;
                        case console::blue:
                            colorStr = light ? "94" : "34";
                            break;
                        case console::yellow:
                            colorStr = light ? "93" : "33";
                            break;
                        case console::purple:
                            colorStr = light ? "95" : "35";
                            break;
                        case console::cyan:
                            colorStr = light ? "96" : "36";
                            break;
                        case console::white:
                            colorStr = light ? "97" : "37";
                            break;
                        default:
                            break;
                    }
                    if (colorStr) {
                        str_push(colorStr);
                    }
                }
                if (bg != _bg) {
                    _bg = bg;

                    bool light = bg & console::intensified;
                    const char *colorStr = nullptr;
                    switch (bg & 0xF) {
                        case console::red:
                            colorStr = light ? "101" : "41";
                            break;
                        case console::green:
                            colorStr = light ? "102" : "42";
                            break;
                        case console::blue:
                            colorStr = light ? "104" : "44";
                            break;
                        case console::yellow:
                            colorStr = light ? "103" : "43";
                            break;
                        case console::purple:
                            colorStr = light ? "105" : "45";
                            break;
                        case console::cyan:
                            colorStr = light ? "106" : "46";
                            break;
                        case console::white:
                            colorStr = light ? "107" : "47";
                            break;
                        default:
                            break;
                    }
                    if (colorStr) {
                        str_push(colorStr);
                    }
                }
                if (style != _style) {
                    _style = style;

                    if (style & console::bold) {
                        str_push("1");
                    }
                    if (style & console::italic) {
                        str_push("3");
                    }
                    if (style & console::underline) {
                        str_push("4");
                    }
                    if (style & console::strikethrough) {
                        str_push("9");
                    }
                }
                if (strListSize > 0) {
                    char buf[20];
                    int bufSize = 0;
                    auto buf_puts = [&buf, &bufSize](const char *s) {
                        for (; *s != '\0'; ++s) {
                            buf[bufSize++] = *s;
                        }
                    };
                    buf_puts("\033[");
                    for (int i = 0; i < strListSize - 1; ++i) {
                        buf_puts(strList[i]);
                        buf_puts(";");
                    }
                    buf_puts(strList[strListSize - 1]);
                    buf_puts("m");
                    buf[bufSize] = '\0';
                    std::ignore = fputs(buf, _file);
                }
            }

            void reset() override {
                if (_fg != _fg_init || _bg != _bg_init || _style != _style_init) {
                    const char *resetColor = "\033[0m";
                    std::ignore = fputs(resetColor, _file);

                    _fg = _fg_init;
                    _bg = _bg_init;
                    _style = _style_init;
                }
            }
        };

#ifdef _WIN32
        class WindowsLegacyOutput : public BaseOutput {
        public:
            WindowsLegacyOutput() {
                _hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
                if (_hConsole != INVALID_HANDLE_VALUE) {
                    ::GetConsoleScreenBufferInfo(_hConsole, &_csbi);
                }
            }
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

                if (needSet && _hConsole != INVALID_HANDLE_VALUE) {
                    ::SetConsoleTextAttribute(_hConsole, winColor);
                }
            }

            void reset() override {
                if (_fg != _fg_init || _bg != _bg_init) {
                    if (_hConsole != INVALID_HANDLE_VALUE) {
                        ::SetConsoleTextAttribute(_hConsole, _csbi.wAttributes);
                    }

                    _fg = _fg_init;
                    _bg = _bg_init;
                }
            }

        private:
            HANDLE _hConsole;
            CONSOLE_SCREEN_BUFFER_INFO _csbi;
        };
#endif

        static BaseOutput &output() {
            static BaseOutput &instance = []() -> BaseOutput & {
#ifdef _WIN32
                bool ensureVTProcessing = false;
                HANDLE hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
                DWORD mode = 0;
                bool suc = ::GetConsoleMode(hConsole, &mode);
                if (::GetConsoleMode(hConsole, &mode)) {
                    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
                        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                        if (::SetConsoleMode(hConsole, mode)) {
                            ensureVTProcessing = true;
                        }
                    } else {
                        ensureVTProcessing = true;
                    }
                }

                if (!ensureVTProcessing) {
                    static WindowsLegacyOutput instance;
                    return instance;
                }
#endif
                static VTOutput instance;
                return instance;
            }();
            return instance;
        }

        ConsoleOutputGuard(FILE *file) {
            output().enter(file);
        }

        ~ConsoleOutputGuard() {
            output().leave();
        }

        void change(int style, int fg, int bg) {
            output().change(style, fg, bg);
        }

        void reset() {
            output().reset();
        }
    };

    /*！
        \namespace console
        \brief Namespace of console related functions.
    */

    namespace console {

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

            llvm::SmallVector<varexp_part, 10> parts;
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
            static constexpr int STACK_BUFFER_SIZE = 4096; // 栈上缓冲区大小

            va_list args_copy;
            va_copy(args_copy, args); // 复制 va_list

            // 第一次尝试：使用栈上缓冲区
            char stack_buffer[STACK_BUFFER_SIZE];
            int len = std::vsnprintf(stack_buffer, STACK_BUFFER_SIZE, fmt, args);
            if (len < 0) {
                va_end(args_copy);
                return -1;
            }

            if (len < STACK_BUFFER_SIZE) {
                // 如果栈上缓冲区足够，直接输出
                len = cfputs(std::string_view(stack_buffer, len), file);
            } else {
                // 如果栈上缓冲区不足，则在堆上分配足够的空间
                char *heap_buffer = new char[len + 1]; // +1 用于 '\0'
                if (!heap_buffer) {
                    va_end(args_copy);
                    return -1;
                }

                // 使用副本重新格式化
                len = std::vsnprintf(heap_buffer, len + 1, fmt, args_copy);
                len = cfputs(std::string_view(heap_buffer, len), file);

                // 释放堆上缓冲区
                delete[] heap_buffer;
            }

            va_end(args_copy); // 结束副本
            return len;
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
