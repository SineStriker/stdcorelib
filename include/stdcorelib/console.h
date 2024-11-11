#ifndef STDCORELIB_CONSOLE_H
#define STDCORELIB_CONSOLE_H

#include <cstdarg>

#include <stdcorelib/strings.h>

namespace stdc {

    namespace console {

        enum color {
            plain = -1,
            black = 0x0,
            red = 0x1,
            green = 0x2,
            blue = 0x4,
            yellow = red | green,
            purple = red | blue,
            cyan = green | blue,
            white = red | green | blue,
            intensified = 0x100,
        };

        STDCORELIB_EXPORT int printf(int foreground, int background, const char *fmt, ...)
            STDCORELIB_PRINTF_FORMAT(3, 4);

        STDCORELIB_EXPORT int vprintf(int foreground, int background, const char *fmt,
                                      va_list args);

        template <class... Args>
        static inline void print(int foreground, int background, const std::string &format,
                                 Args &&...args) {
            printf(foreground, background, "%s", formatN(format, args...).c_str());
        }

        template <class... Args>
        static inline void println(int foreground, int background, const std::string &format,
                                   Args &&...args) {
            printf(foreground, background, "%s\n", formatN(format, args...).c_str());
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

    using console::u8printf;
    using console::u8vprintf;
    using console::u8print;
    using console::u8println;

}

#endif // STDCORELIB_CONSOLE_H