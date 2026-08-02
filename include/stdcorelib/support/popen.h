#ifndef STDCORELIB_EXPERIMENTAL_POPEN_H
#define STDCORELIB_EXPERIMENTAL_POPEN_H

#include <filesystem>
#include <vector>
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

        // Closes this side of the child's stdin, which is how the child is told there is no more
        // input. A child reading to end of input will not exit until this happens.
        void close_stdin();

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

        FILE *stdin_() const;
        FILE *stdout_() const;
        FILE *stderr_() const;

        int pid() const;
        std::optional<int> returncode() const;

    protected:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // POPEN_H