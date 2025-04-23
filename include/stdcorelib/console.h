#ifndef STDCORELIB_CONSOLE_H
#define STDCORELIB_CONSOLE_H

#include <cstdarg>

#include <stdcorelib/str.h>


namespace stdc {

    namespace console {

        enum style {
            nostyle = 0x0,
            bold = 0x1,
            italic = 0x2,
            underline = 0x4,
            strikethrough = 0x8,
        };

        enum color {
            nocolor = 0,
            intensified = 0x10,

            red = 0x1,
            green = 0x2,
            blue = 0x4,
            yellow = red | green,
            purple = red | blue,
            cyan = green | blue,
            white = red | green | blue,
            black = 0x8,
            lightred = intensified | red,
            lightgreen = intensified | red,
            lightblue = intensified | blue,
            lightyellow = intensified | yellow,
            lightpurple = intensified | purple,
            lightcyan = intensified | cyan,
            lightwhite = intensified | white,
            lightblack = intensified | black,
        };

        STDCORELIB_EXPORT int printf(int style, int fg, int bg, const char *fmt, ...)
            STDCORELIB_PRINTF_FORMAT(4, 5);

        STDCORELIB_EXPORT int vprintf(int style, int fg, int bg, const char *fmt, va_list args);

        template <class... Args>
        static inline void print(int fg, int bg, const std::string &format, Args &&...args) {
            printf(fg, bg, "%s", formatN(format, args...).c_str());
        }

        template <class... Args>
        static inline void println(int fg, int bg, const std::string &format, Args &&...args) {
            printf(fg, bg, "%s\n", formatN(format, args...).c_str());
        }

        STDCORELIB_EXPORT int u8printf(const char *fmt, ...) STDCORELIB_PRINTF_FORMAT(1, 2);

        STDCORELIB_EXPORT int u8vprintf(const char *fmt, va_list args);

        template <class... Args>
        inline void u8print(const std::string &format, Args &&...args) {
            u8printf("%s", formatN(format, args...).c_str());
        }

        template <class... Args>
        inline void u8println(const std::string &format, Args &&...args) {
            u8printf("%s\n", formatN(format, args...).c_str());
        }

        inline void u8println() {
            u8printf("\n");
        }

    }

    using console::u8print;
    using console::u8printf;
    using console::u8println;
    using console::u8vprintf;

}

#endif // STDCORELIB_CONSOLE_H