#ifndef STDCORELIB_CONSOLE_H
#define STDCORELIB_CONSOLE_H

#include <cstdarg>
#include <cstdio>

#include <stdcorelib/str.h>

namespace stdc {

    namespace console {

        /// Text attributes. Several may be combined with |.
        enum style {
            nostyle = 0x0,
            bold = 0x1,
            italic = 0x2,
            underline = 0x4,
            strikethrough = 0x8,
        };

        /// The eight base colors, each with a brighter variant. Only one at a time.
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

        /// \name Color mode
        /// @{

        /// How styling reaches the target.
        enum color_mode {
            automatic,      // decide per target: style it only when it is a terminal
            never,          // never emit styling, whatever the target
            vt,             // always emit ANSI escape sequences
            windows_legacy, // always drive the Windows console API, means `never` elsewhere
        };

        STDCORELIB_EXPORT color_mode get_color_mode();

        /// Overrides the mode process wide, which is where a --color=always or NO_COLOR flag
        /// belongs. Also drops what has been detected about the targets seen so far, so call it
        /// again with the current mode after a freopen() to force them to be probed anew.
        STDCORELIB_EXPORT void set_color_mode(color_mode mode);

        /// Returns the mode that will actually be used for \a file. Never returns `automatic`.
        STDCORELIB_EXPORT color_mode resolve_color_mode(FILE *file);

        /// @}

        /// \name General output
        /// @{

        /// Writes \a buf with the given attributes and puts them back afterwards. \a style is a
        /// bitwise or of `style` values, \a fg and \a bg are one `color` each. Whether anything
        /// is emitted at all is up to resolve_color_mode() for that file, so a redirected stream
        /// gets the text alone.
        ///
        /// The string is taken as UTF-8 and transcoded for a Windows console.
        STDCORELIB_EXPORT int fputs(int style, int fg, int bg, const char *buf, FILE *file);

        // @overload: fputs
        STDCORELIB_EXPORT int fputs(int style, int fg, int bg, const std::string_view &buf,
                                    FILE *file);

        /// Like fputs(), to stdout and followed by a newline.
        STDCORELIB_EXPORT int puts(int style, int fg, int bg, const char *buf);

        // @overload: puts
        STDCORELIB_EXPORT int puts(int style, int fg, int bg, const std::string_view &buf);

        /// Like fputs(), with printf-style formatting.
        STDCORELIB_EXPORT int fprintf(int style, int fg, int bg, FILE *file, const char *fmt, ...)
            STDCORELIB_PRINTF_FORMAT(5, 6);

        STDCORELIB_EXPORT int vfprintf(int style, int fg, int bg, FILE *file, const char *fmt,
                                       va_list args);

        STDCORELIB_EXPORT int printf(int style, int fg, int bg, const char *fmt, ...)
            STDCORELIB_PRINTF_FORMAT(4, 5);

        STDCORELIB_EXPORT int vprintf(int style, int fg, int bg, const char *fmt, va_list args);

        /// Like fputs(), with formatN() placeholders (%1, %2, ...) rather than printf ones.
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

        /// @}

        /// \name Plain output
        /// @{

        /// The same writers with no attributes at all. Still worth preferring over std::fputs on
        /// Windows, where the console needs UTF-8 text transcoded before it will render.
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

        /// @}

        /// \name Messages
        /// @{

        /// One line each in a conventional color, for programs that want the four usual
        /// severities without picking colors themselves. Formatting is formatN()'s.
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

        /// @}
    }

    using console::u8printf;
    using console::u8vprintf;
    using console::u8print;
    using console::u8println;

    namespace console {

        /// \name Inline color markup
        /// @{

        /// Writes \a buf, reading `${...}` as attribute changes rather than as text. This is the
        /// alternative to threading style, fg and bg arguments through every call.
        ///
        /// A group holds one or more names separated by spaces, and `$$` writes a literal `$`.
        /// The names are:
        ///   - a color: red, green, blue, yellow, purple, cyan, white, black, nocolor, each also
        ///     available with a `light` prefix
        ///   - the same again behind `@`, which sets the background instead of the foreground
        ///   - a style: bold, italic, underline, strikethrough, nostyle
        ///   - intensified, or @intensified, to brighten whichever color is already in effect
        ///   - reset, or clear, to drop back to plain text
        ///
        /// A name that is none of these is dropped and changes nothing. Attributes start out
        /// plain on every call and are restored when it returns, so they never leak into what is
        /// written next.
        ///
        /// \code
        ///   cprintln("${lightgreen}ok ${@blue bold}on blue ${reset}plain, 50$$ off");
        /// \endcode
        STDCORELIB_EXPORT int cfputs(const char *buf, FILE *file);

        // @overload: cfputs
        STDCORELIB_EXPORT int cfputs(const std::string_view &buf, FILE *file);

        /// Like cfputs(), to stdout and followed by a newline.
        STDCORELIB_EXPORT int cputs(const char *buf);

        // @overload: cputs
        STDCORELIB_EXPORT int cputs(const std::string_view &buf);

        /// Like cfputs(), with printf-style formatting. A `%` conversion may produce a `${...}`
        /// group of its own, which is then read the same way.
        STDCORELIB_EXPORT int cfprintf(FILE *file, const char *fmt, ...)
            STDCORELIB_PRINTF_FORMAT(2, 3);

        STDCORELIB_EXPORT int cvfprintf(FILE *file, const char *fmt, va_list args);

        STDCORELIB_EXPORT int cprintf(const char *fmt, ...) STDCORELIB_PRINTF_FORMAT(1, 2);

        STDCORELIB_EXPORT int cvprintf(const char *fmt, va_list args);

        /// Like cfputs(), with formatN() placeholders (%1, %2, ...).
        template <class... Args>
        inline int cprint(const std::string_view &format, Args &&...args) {
            return cfputs(formatN(format, std::forward<Args>(args)...), stdout);
        }

        template <class... Args>
        inline int cprintln(const std::string_view &format, Args &&...args) {
            return cputs(formatN(format, std::forward<Args>(args)...));
        }

        /// @}
    }

    using console::cprintf;
    using console::cvprintf;
    using console::cprint;
    using console::cprintln;

}

#endif // STDCORELIB_CONSOLE_H