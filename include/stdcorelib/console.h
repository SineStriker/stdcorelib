#ifndef STDCORELIB_CONSOLE_H
#define STDCORELIB_CONSOLE_H

#include <cstdarg>
#include <cstdio>

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
            lightgreen = intensified | green,
            lightblue = intensified | blue,
            lightyellow = intensified | yellow,
            lightpurple = intensified | purple,
            lightcyan = intensified | cyan,
            lightwhite = intensified | white,
            lightblack = intensified | black,
        };

        //
        // General APIs
        //
        STDCORELIB_EXPORT int fputs(int style, int fg, int bg, const char *buf, FILE *file);

        // @overload: fputs
        STDCORELIB_EXPORT int fputs(int style, int fg, int bg, const std::string_view &buf,
                                    FILE *file);

        STDCORELIB_EXPORT int puts(int style, int fg, int bg, const char *buf);

        // @overload: puts
        STDCORELIB_EXPORT int puts(int style, int fg, int bg, const std::string_view &buf);

        STDCORELIB_EXPORT int fprintf(int style, int fg, int bg, FILE *file, const char *fmt, ...)
            STDCORELIB_PRINTF_FORMAT(5, 6);

        STDCORELIB_EXPORT int vfprintf(int style, int fg, int bg, FILE *file, const char *fmt,
                                       va_list args);

        STDCORELIB_EXPORT int printf(int style, int fg, int bg, const char *fmt, ...)
            STDCORELIB_PRINTF_FORMAT(4, 5);

        STDCORELIB_EXPORT int vprintf(int style, int fg, int bg, const char *fmt, va_list args);

        template <class... Args>
        inline int print(int style, int fg, int bg, const std::string_view &format,
                         Args &&...args) {
            return console::fputs(style, fg, bg, formatN(format, args...), stdout);
        }

        template <class... Args>
        inline int println(int style, int fg, int bg, const std::string_view &format,
                           Args &&...args) {
            return console::puts(style, fg, bg, formatN(format, std::forward<Args>(args)...));
        }
        
        // @overload: println
        inline int println() {
            return std::putchar('\n');
        }

        //
        // Plain APIs (Use UTF-8 as prefix)
        //
        inline int u8fputs(const char *buf, FILE *file) {
            return console::fputs(nostyle, nocolor, nocolor, buf, file);
        }

        // @overload: u8fputs
        inline int u8fputs(const std::string_view &buf, FILE *file) {
            return console::fputs(nostyle, nocolor, nocolor, buf, file);
        }

        inline int u8puts(const char *buf) {
            return console::puts(nostyle, nocolor, nocolor, buf);
        }

        // @overload: u8puts
        inline int u8puts(const std::string_view &buf) {
            return console::puts(nostyle, nocolor, nocolor, buf);
        }

        STDCORELIB_EXPORT int u8fprintf(FILE *file, const char *fmt, ...)
            STDCORELIB_PRINTF_FORMAT(2, 3);

        STDCORELIB_EXPORT int u8vfprintf(FILE *file, const char *fmt, va_list args);

        STDCORELIB_EXPORT int u8printf(const char *fmt, ...) STDCORELIB_PRINTF_FORMAT(1, 2);

        STDCORELIB_EXPORT int u8vprintf(const char *fmt, va_list args);

        template <class... Args>
        inline int u8print(const std::string_view &format, Args &&...args) {
            return u8fputs(formatN(format, std::forward<Args>(args)...), stdout);
        }

        template <class... Args>
        inline int u8println(const std::string_view &format, Args &&...args) {
            return u8puts(formatN(format, std::forward<Args>(args)...));
        }

        // @overload: u8println
        inline int u8println() {
            return std::putchar('\n');
        }

        //
        // Message APIs
        //
        template <class... Args>
        inline int debug(const std::string_view &format, Args &&...args) {
            return println(nostyle, lightblue, nocolor, format, std::forward<Args>(args)...);
        }

        template <class... Args>
        inline int success(const std::string_view &format, Args &&...args) {
            return println(nostyle, lightgreen, nocolor, format, std::forward<Args>(args)...);
        }

        template <class... Args>
        inline int warning(const std::string_view &format, Args &&...args) {
            return println(nostyle, yellow, nocolor, format, std::forward<Args>(args)...);
        }

        template <class... Args>
        inline int critical(const std::string_view &format, Args &&...args) {
            return println(nostyle, red, nocolor, format, std::forward<Args>(args)...);
        }

    }

    using console::u8printf;
    using console::u8vprintf;
    using console::u8print;
    using console::u8println;

    namespace console {

        //
        // Color APIs
        //
        STDCORELIB_EXPORT int cfputs(const char *buf, FILE *file);

        // @overload: cfputs
        STDCORELIB_EXPORT int cfputs(const std::string_view &buf, FILE *file);

        STDCORELIB_EXPORT int cputs(const char *buf);

        // @overload: cputs
        STDCORELIB_EXPORT int cputs(const std::string_view &buf);

        STDCORELIB_EXPORT int cfprintf(FILE *file, const char *fmt, ...)
            STDCORELIB_PRINTF_FORMAT(2, 3);

        STDCORELIB_EXPORT int cvfprintf(FILE *file, const char *fmt, va_list args);

        STDCORELIB_EXPORT int cprintf(const char *fmt, ...) STDCORELIB_PRINTF_FORMAT(1, 2);

        STDCORELIB_EXPORT int cvprintf(const char *fmt, va_list args);

        template <class... Args>
        inline int cprint(const std::string_view &format, Args &&...args) {
            return cfputs(formatN(format, std::forward<Args>(args)...), stdout);
        }

        template <class... Args>
        inline int cprintln(const std::string_view &format, Args &&...args) {
            return cputs(formatN(format, std::forward<Args>(args)...));
        }

    }

    using console::cprintf;
    using console::cvprintf;
    using console::cprint;
    using console::cprintln;

}

#endif // STDCORELIB_CONSOLE_H