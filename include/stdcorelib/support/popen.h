#ifndef STDCORELIB_EXPERIMENTAL_POPEN_H
#define STDCORELIB_EXPERIMENTAL_POPEN_H

#include <filesystem>
#include <vector>
#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <system_error>
#include <functional>

#include <stdcorelib/stdc_global.h>
#include <stdcorelib/adt/array_view.h>

namespace stdc {

    class STDCORELIB_EXPORT Popen {
    public:
        enum IOType {
            PIPE = 1,
            DEVNULL,
            STDOUT,
        };

        struct IODev {
            enum Kind {
                None,
                Builtin,
                FD,
                CFile,
            };
            IODev() : kind(None) {
            }
            IODev(IOType builtin) : kind(Builtin) {
                data.builtin = builtin;
            }
            IODev(int fd) : kind(FD) {
                data.fd = fd;
            }
            IODev(FILE *file) : kind(CFile) {
                data.file = file;
            }
            int kind;
            union {
                IOType builtin;
                int fd;
                FILE *file;
            } data;
        };

#ifdef _WIN32
        struct StartupInfo {
            // winapi members
            uint32_t dwFlags;
            void *hStdInput;
            void *hStdOutput;
            void *hStdError;
            uint16_t wShowWindow;

            // supported keys:
            //     handle_list: INVALID_HANDLE_VALUE terminated list of HANDLE to be inherited
            std::map<std::string, void *> lpAttributeList;
        };

        enum WindowsSignal {
            WS_CTRL_C_EVENT = 0,
            WS_CTRL_BREAK_EVENT = 1,
        };
#endif

        // One end of a pipe to the child, as a C++ stream.
        //
        // The direction is fixed by which pipe it is: stdin_() is written to, stdout_() and
        // stderr_() are read from. Using one the wrong way round fails at run time rather than
        // at compile time.
        //
        // The stream owns what it is reading or writing, so close() can be called more than once
        // and the destructor tidies up whatever is left. That is the difference from handing out
        // a bare FILE *: a closed FILE * cannot be told apart from an open one, and the slot is
        // recycled by the next fopen.
        class STDCORELIB_EXPORT Stream : public std::iostream {
        public:
            Stream();
            ~Stream() override;

            // Flushes and closes. Doing it twice is harmless. Closing stdin_() is what tells a
            // child reading to end of input that there is no more coming.
            void close();
            bool is_open() const;

            // The underlying handle, for the C interfaces that only take one -- fprintf, or any
            // library with a FILE * entry point. It is borrowed: let close() do the closing.
            // Null once closed, or when this pipe was never opened.
            FILE *file() const;

        private:
            friend class Popen;
            void open(FILE *file);

            class Buf;
            std::unique_ptr<Buf> _buf;

            STDCORELIB_DISABLE_COPY_MOVE(Stream)
        };

        Popen();
        ~Popen();

        Popen(Popen &&RHS) noexcept;
        Popen &operator=(Popen &&RHS) noexcept;

    public:
        //
        // input: shouldn't call after calling start()
        //
        Popen &executable(const std::filesystem::path &executable);
        Popen &args(const std::vector<std::string> &args);
        Popen &shell(bool shell);

        Popen &cwd(const std::filesystem::path &cwd);
        Popen &env(const std::map<std::string, std::string> &env);

        Popen &stdin_(IODev dev);
        Popen &stdout_(IODev dev);
        Popen &stderr_(IODev dev);

        Popen &text(bool text);
        Popen &close_fds(bool close_fds);
        Popen &pipesize(int pipesize); // linux only (ignored on other platforms)

#ifdef _WIN32
        Popen &startupinfo(const StartupInfo *startupinfo); // windows only
        Popen &creationflags(int creationflags);            // windows only
#else
        Popen &preexec_fn(const std::function<void()> &preexec_fn); // unix only
        Popen &restore_signals(bool restore_signals);               // unix only
        Popen &start_new_session(bool start_new_session);           // unix only
        Popen &pass_fds(const std::vector<int> &pass_fds);          // unix only
        Popen &group(int group);                                    // unix only
        Popen &extra_groups(const std::vector<int> &extra_groups);  // unix only
        Popen &user(int user);                                      // unix only
        Popen &user(const char *user);                              // unix only
        Popen &umask(int umask);                                    // unix only
        Popen &process_group(int process_group);                    // unix only
#endif

    public:
        //
        // additional apis
        //
        bool start(std::string *err_msg = nullptr);
        std::error_code error_code() const;

    public:
        //
        // methods
        //
        bool poll();
        bool wait(int timeout = -1);

        std::tuple<std::string, std::string> communicate(const std::string &input = {},
                                                         int timeout = -1);
        bool send_signal(int sig);

        // Asks the process to close, the way QProcess::terminate does: WM_CLOSE to its windows on
        // Windows, SIGTERM elsewhere. A process may ignore it, and one without a message loop --
        // any console program -- will not notice at all. kill() is the one that cannot be refused.
        bool terminate();
        bool kill();

    public:
        //
        // properties
        //
        const std::filesystem::path &executable() const;
        array_view<std::string> args() const;

        // The pipes, when the matching stream was set to PIPE. Otherwise is_open() is false.
        Stream &stdin_() const;
        Stream &stdout_() const;
        Stream &stderr_() const;

        int pid() const;
        std::optional<int> returncode() const;

    protected:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // POPEN_H