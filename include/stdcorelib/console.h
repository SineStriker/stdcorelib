#ifndef STDCORELIB_CONSOLE_H
#define STDCORELIB_CONSOLE_H

#include <cstdarg>

#include <stdcorelib/global.h>
#include <stdcorelib/format.h>

namespace stdc {

    class STDCORELIB_EXPORT Console {
    public:
        enum Color {
            Default = -1,
            Black = 0x0,
            Red = 0x1,
            Green = 0x2,
            Blue = 0x4,
            Yellow = Red | Green,
            Purple = Red | Blue,
            Cyan = Green | Blue,
            White = Red | Green | Blue,
            Intensified = 0x100,
        };

        static int printf(int foreground, int background, const char *fmt, ...)
            STDCORELIB_PRINTF_FORMAT(3, 4);

        static int vprintf(int foreground, int background, const char *fmt, va_list args);

        template <class... Args>
        static inline void print(int foreground, int background, const std::string &format,
                                 Args &&...args) {
            printf(foreground, background, "%s\n", formatTextN(format, args...).c_str());
        }
    };

    STDCORELIB_EXPORT int u8printf(const char *fmt, ...) STDCORELIB_PRINTF_FORMAT(1, 2);

    STDCORELIB_EXPORT int u8vprintf(const char *fmt, va_list args);

    template <class... Args>
    static inline void u8print(const std::string &format, Args &&...args) {
        u8printf("%s\n", formatTextN(format, args...).c_str());
    }

}

#endif // STDCORELIB_CONSOLE_H