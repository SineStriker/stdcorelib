#include "console.h"

#ifdef _WIN32
#  include <windows.h>
#endif

#include <mutex>
#include <cstdio>
#include <cstring>

namespace stdc {

    class ConsoleOutputGuard {
    public:
        class BaseOutput {
        public:
            BaseOutput() = default;
            virtual ~BaseOutput() = default;

            void enter(int style, int fg, int bg) {
                global_mtx().lock();
                _modeChanged = !(style == console::nostyle && fg == console::nocolor &&
                                 bg == console::nocolor);
#ifdef _WIN32
                _codepage = ::GetConsoleOutputCP();
                ::SetConsoleOutputCP(CP_UTF8);
#endif
                enterImpl(style, fg, bg);
            }

            void leave() {
                leaveImpl();
#ifdef _WIN32
                ::SetConsoleOutputCP(_codepage);
#endif
                global_mtx().unlock();
            }

        protected:
            bool _modeChanged{};

            virtual void enterImpl(int style, int fg, int bg) = 0;
            virtual void leaveImpl() = 0;

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
            VTOutput() {
            }
            void enterImpl(int style, int fg, int bg) override {
                if (_modeChanged) {
                    const char *strList[10];
                    int strListSize = 0;
                    auto str_push = [&](const char *s) {
                        strList[strListSize] = s;
                        strListSize++;
                    };
                    if (fg != console::nocolor) {
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
                    if (bg != console::nocolor) {
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
                        printf("%s", buf);
                    }
                }
            }

            void leaveImpl() override {
                if (_modeChanged) {
                    const char *resetColor = "\033[0m";
                    printf("%s", resetColor);
                }
            }
        };

#ifdef _WIN32
        class WindowsLegacyOutput : public BaseOutput {
        public:
            WindowsLegacyOutput() {
                _hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
            }
            void enterImpl(int style, int fg, int bg) override {
                if (_modeChanged) {
                    WORD winColor = 0;
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
                    ::GetConsoleScreenBufferInfo(_hConsole, &_csbi);
                    ::SetConsoleTextAttribute(_hConsole, winColor);
                }
            }

            void leaveImpl() override {
                if (_modeChanged) {
                    ::SetConsoleTextAttribute(_hConsole, _csbi.wAttributes);
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

        ConsoleOutputGuard(int style, int fg, int bg) {
            output().enter(style, fg, bg);
        }

        ~ConsoleOutputGuard() {
            output().leave();
        }
    };

    /*！
        \namespace console
        \brief Namespace of console related functions.
    */

    namespace console {

        /*!
            Print formatted string in UTF-8 encoding with specified styles.
        */
        int printf(int style, int fg, int bg, const char *fmt, ...) {
            ConsoleOutputGuard _guard(style, fg, bg);

            va_list args;
            va_start(args, fmt);
            int res = std::vprintf(fmt, args);
            va_end(args);
            return res;
        }

        /*!
            Print formatted string in UTF-8 encoding with specified styles.
        */
        int vprintf(int style, int fg, int bg, const char *fmt, va_list args) {
            ConsoleOutputGuard _guard(style, fg, bg);
            return std::vprintf(fmt, args);
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

        /*!
            Print formatted string in UTF-8 encoding with default styles.
        */
        int u8printf(const char *fmt, ...) {
            ConsoleOutputGuard _guard(console::nostyle, console::nocolor, console::nocolor);

            va_list args;
            va_start(args, fmt);
            int res = std::vprintf(fmt, args);
            va_end(args);
            return res;
        }

        /*!
            Print formatted string in UTF-8 encoding with default styles.
        */
        int u8vprintf(const char *fmt, va_list args) {
            ConsoleOutputGuard _guard(console::nostyle, console::nocolor, console::nocolor);
            return std::vprintf(fmt, args);
        }

    }

}
