#include "console.h"

#ifdef _WIN32
#  include <windows.h>
#endif

#include <mutex>
#include <cstdio>
#include <cstring>

namespace stdc {

    class PrintScopeGuard {
    public:
        static std::mutex &global_mtx() {
            static std::mutex _instance;
            return _instance;
        }

        explicit PrintScopeGuard(int foreground, int background)
            : consoleChanged(!(foreground == Console::Default && background == Console::Default)) {
            global_mtx().lock();
#ifdef _WIN32
            _codepage = ::GetConsoleCP();
            ::SetConsoleCP(CP_UTF8);

            if (consoleChanged) {
                WORD winColor = 0;
                if (foreground != Console::Default) {
                    winColor |= (foreground & Console::Intensified) ? FOREGROUND_INTENSITY : 0;
                    switch (foreground & 0xF) {
                        case Console::Red:
                            winColor |= FOREGROUND_RED;
                            break;
                        case Console::Green:
                            winColor |= FOREGROUND_GREEN;
                            break;
                        case Console::Blue:
                            winColor |= FOREGROUND_BLUE;
                            break;
                        case Console::Yellow:
                            winColor |= FOREGROUND_RED | FOREGROUND_GREEN;
                            break;
                        case Console::Purple:
                            winColor |= FOREGROUND_RED | FOREGROUND_BLUE;
                            break;
                        case Console::Cyan:
                            winColor |= FOREGROUND_GREEN | FOREGROUND_BLUE;
                            break;
                        case Console::White:
                            winColor |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
                        default:
                            break;
                    }
                }
                if (background != Console::Default) {
                    winColor |= (background & Console::Intensified) ? BACKGROUND_INTENSITY : 0;
                    switch (background & 0xF) {
                        case Console::Red:
                            winColor |= BACKGROUND_RED;
                            break;
                        case Console::Green:
                            winColor |= BACKGROUND_GREEN;
                            break;
                        case Console::Blue:
                            winColor |= BACKGROUND_BLUE;
                            break;
                        case Console::Yellow:
                            winColor |= BACKGROUND_RED | BACKGROUND_GREEN;
                            break;
                        case Console::Purple:
                            winColor |= BACKGROUND_RED | BACKGROUND_BLUE;
                            break;
                        case Console::Cyan:
                            winColor |= BACKGROUND_GREEN | BACKGROUND_BLUE;
                            break;
                        case Console::White:
                            winColor |= BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
                        default:
                            break;
                    }
                }
                _hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                GetConsoleScreenBufferInfo(_hConsole, &_csbi);
                SetConsoleTextAttribute(_hConsole, winColor);
            }
#else
            if (consoleChanged) {
                const char *strList[3];
                int strListSize = 0;
                if (foreground != Console::Default) {
                    bool light = foreground & Console::Intensified;
                    const char *colorStr = nullptr;
                    switch (foreground & 0xF) {
                        case Console::Red:
                            colorStr = light ? "91" : "31";
                            break;
                        case Console::Green:
                            colorStr = light ? "92" : "32";
                            break;
                        case Console::Blue:
                            colorStr = light ? "94" : "34";
                            break;
                        case Console::Yellow:
                            colorStr = light ? "93" : "33";
                            break;
                        case Console::Purple:
                            colorStr = light ? "95" : "35";
                            break;
                        case Console::Cyan:
                            colorStr = light ? "96" : "36";
                            break;
                        case Console::White:
                            colorStr = light ? "97" : "37";
                            break;
                        default:
                            break;
                    }
                    if (colorStr) {
                        strList[strListSize] = colorStr;
                        strListSize++;
                    }
                }
                if (background != Console::Default) {
                    bool light = background & Console::Intensified;
                    const char *colorStr = nullptr;
                    switch (background & 0xF) {
                        case Console::Red:
                            colorStr = light ? "101" : "41";
                            break;
                        case Console::Green:
                            colorStr = light ? "102" : "42";
                            break;
                        case Console::Blue:
                            colorStr = light ? "104" : "44";
                            break;
                        case Console::Yellow:
                            colorStr = light ? "103" : "43";
                            break;
                        case Console::Purple:
                            colorStr = light ? "105" : "45";
                            break;
                        case Console::Cyan:
                            colorStr = light ? "106" : "46";
                            break;
                        case Console::White:
                            colorStr = light ? "107" : "47";
                            break;
                        default:
                            break;
                    }
                    if (colorStr) {
                        strList[strListSize] = colorStr;
                        strListSize++;
                    }
                }
                if (strListSize > 0) {
                    char buf[20];
                    int bufSize = 0;
                    auto buf_puts = [&buf, &bufSize](const char *s) {
                        auto len = strlen(s);
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
                    printf("%s", buf);
                }
            }
#endif
        }

        ~PrintScopeGuard() {
#ifdef _WIN32
            ::SetConsoleCP(_codepage);

            if (consoleChanged) {
                SetConsoleTextAttribute(_hConsole, _csbi.wAttributes);
            }
#else
            if (consoleChanged) {
                // ANSI escape code to reset text color to default
                const char *resetColor = "\033[0m";
                printf("%s", resetColor);
            }
#endif
            global_mtx().unlock();
        }

    private:
        bool consoleChanged;
#ifdef _WIN32
        UINT _codepage;
        HANDLE _hConsole;
        CONSOLE_SCREEN_BUFFER_INFO _csbi;
#endif
    };

    /*!
        Print formatted string in UTF-8 encoding with specified colors.
    */
    int Console::printf(int foreground, int background, const char *fmt, ...) {
        PrintScopeGuard _guard(foreground, background);

        va_list args;
        va_start(args, fmt);
        int res = std::vprintf(fmt, args);
        va_end(args);
        return res;
    }

    /*!
        Print formatted string in UTF-8 encoding with specified colors.
    */
    int Console::vprintf(int foreground, int background, const char *fmt, va_list args) {
        PrintScopeGuard _guard(foreground, background);
        return std::vprintf(fmt, args);
    }

    /*!
        \fn void print(int foreground, int background, const std::string &format, Args &&...args)
        
        Print formatted string in UTF-8 encoding with specified colors.
    */

    /*!
        Print formatted string in UTF-8 encoding with default colors.
    */
    int u8printf(const char *fmt, ...) {
        PrintScopeGuard _guard(Console::Default, Console::Default);

        va_list args;
        va_start(args, fmt);
        int res = std::vprintf(fmt, args);
        va_end(args);
        return res;
    }

    /*!
        Print formatted string in UTF-8 encoding with default colors.
    */
    int u8vprintf(const char *fmt, va_list args) {
        PrintScopeGuard _guard(Console::Default, Console::Default);
        return std::vprintf(fmt, args);
    }

}