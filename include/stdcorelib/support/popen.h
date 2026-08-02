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

    /// Creates and controls a child process, following Python's subprocess.Popen.
    ///
    /// The setters build the process up and return *this, so they chain. Nothing happens until
    /// start(). After that the child is running and the streams that were set to PIPE are open.
    ///
    /// \code
    ///   Popen proc;
    ///   proc.args({"git", "--version"})
    ///       .stdin_(Popen::DEVNULL)
    ///       .stdout_(Popen::PIPE)
    ///       .stderr_(Popen::STDOUT); // fold stderr into the stdout pipe
    ///
    ///   std::string err;
    ///   if (!proc.start(&err)) {
    ///       return err;
    ///   }
    ///   auto [out, _] = proc.communicate();
    ///   int code = proc.returncode().value_or(-1);
    /// \endcode
    ///
    /// Reading a pipe by hand instead of through communicate() works, but only one of them at a
    /// time. A pipe blocks its writer once full, so a child that fills stderr while the parent
    /// is still draining stdout will wait forever. communicate() exists to get this right.
    ///
    /// \sa https://docs.python.org/3/library/subprocess.html
    class STDCORELIB_EXPORT Popen {
    public:
        /// What to connect a standard stream to, beyond a descriptor or a FILE * of your own.
        enum IOType {
            PIPE = 1, ///< a new pipe, readable or writable from this side afterwards
            DEVNULL,  ///< the null device
            STDOUT,   ///< stderr only: send it wherever stdout goes
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

        /// One end of a pipe to the child, as an ordinary iostream. Only open for a stream that
        /// was set to PIPE.
        class STDCORELIB_EXPORT Stream : public std::iostream {
        public:
            Stream();
            ~Stream() override;

            /// Closes this end. On the child's stdin this is what signals end of input, without
            /// which a child that reads to EOF never finishes. Doing it twice is harmless.
            void close();

            bool is_open() const;

            /// The same pipe as a FILE *, for the C interfaces that take nothing else. Owned by
            /// the Stream, so do not fclose it.
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
        /// \name Setup
        ///
        /// All of these take effect at start() and mean nothing after it.
        /// @{

        /// The program to run. Defaults to args()[0], so it is only needed to run a program
        /// under a name of its own choosing.
        Popen &executable(const std::filesystem::path &executable);

        /// The argument vector, argv[0] included. A name with no separator in it is looked up
        /// along PATH.
        Popen &args(const std::vector<std::string> &args);

        /// Hands the command to the system shell rather than executing it directly, so its
        /// redirections and expansions apply. Also its quoting, which is why untrusted input has
        /// no business here.
        Popen &shell(bool shell);

        /// The child's working directory. Inherited if left unset.
        Popen &cwd(const std::filesystem::path &cwd);

        /// The child's environment, which replaces ours rather than adding to it. Inherited if
        /// left unset.
        Popen &env(const std::map<std::string, std::string> &env);

        /// Where each standard stream goes: a PIPE to talk over, DEVNULL to discard, a
        /// descriptor or FILE * to hand it somewhere of your own, or STDOUT on stderr_() to fold
        /// the two together. Inherited if left unset.
        Popen &stdin_(IODev dev);
        Popen &stdout_(IODev dev);
        Popen &stderr_(IODev dev);

        /// Opens the pipes in text mode, which on Windows translates between CRLF and LF as
        /// they are read and written. Nothing changes elsewhere.
        Popen &text(bool text);

        /// Whether the child starts with only the standard streams open. On by default, so a
        /// descriptor of ours is not left in a process that never asked for it.
        Popen &close_fds(bool close_fds);

        Popen &pipesize(int pipesize); // linux only (ignored on other platforms)

#ifdef _WIN32
        Popen &startupinfo(const StartupInfo *startupinfo); // windows only
        Popen &creationflags(int creationflags);            // windows only
#else
        /// Runs in the child after the pipes are in place and before exec. It shares nothing
        /// with this process any more, so anything that could block on a lock another thread
        /// held at fork time may deadlock there.
        Popen &preexec_fn(const std::function<void()> &preexec_fn); // unix only

        /// Puts the signal dispositions this process changed back to their defaults, so the
        /// child does not inherit an ignored SIGPIPE it never asked for. On by default.
        Popen &restore_signals(bool restore_signals); // unix only

        Popen &start_new_session(bool start_new_session); // unix only

        /// Descriptors to leave open across exec despite close_fds. Setting this forces
        /// close_fds on.
        Popen &pass_fds(const std::vector<int> &pass_fds); // unix only

        /// Credentials for the child, all of which need privilege. start() fails otherwise.
        Popen &group(int group);                                   // unix only
        Popen &extra_groups(const std::vector<int> &extra_groups); // unix only
        Popen &user(int user);                                     // unix only
        Popen &user(const char *user);                             // unix only

        Popen &umask(int umask);                 // unix only
        Popen &process_group(int process_group); // unix only
#endif

        /// @}

    public:
        /// \name Starting
        /// @{

        /// Starts the process. On failure returns false and writes the details to \a err_msg,
        /// leaving the reason in error_code().
        bool start(std::string *err_msg = nullptr);

        /// The error from the last operation, cleared at the start of each one.
        std::error_code error_code() const;

        /// @}

    public:
        /// \name Waiting
        /// @{

        /// Whether the child has exited, without waiting for it. False also means "still
        /// running", which is not an error, so tell the two apart by returncode().
        bool poll();

        /// Waits for the child to exit, up to \a timeout milliseconds, or forever if negative.
        /// Returns false on timeout. The pipes stay readable afterwards.
        bool wait(int timeout = -1);

        /// Writes \a input to the child, reads stdout and stderr to the end, and waits. This is
        /// the only safe way to do all three, since draining one pipe at a time deadlocks as
        /// soon as the other one fills.
        ///
        /// Closes stdin after writing, which is what lets a child reading to end of input
        /// finish. A child still running at \a timeout is killed rather than left behind, and
        /// error_code() then says so.
        std::tuple<std::string, std::string> communicate(const std::string &input = {},
                                                         int timeout = -1);

        bool send_signal(int sig);

        /// Requests the process to close, like QProcess::terminate. Posts WM_CLOSE to its windows
        /// on Windows and sends SIGTERM elsewhere. The process may ignore it. A console program
        /// has no message loop and never sees it. Use kill() to force it.
        bool terminate();

        /// Ends the process outright, which it cannot refuse. Anything it was part way through
        /// writing is lost.
        bool kill();

        /// @}

    public:
        /// \name Properties
        /// @{

        const std::filesystem::path &executable() const;
        array_view<std::string> args() const;

        /// Returns the pipe for the stream. Not open unless it was set to PIPE.
        Stream &stdin_() const;
        Stream &stdout_() const;
        Stream &stderr_() const;

        int pid() const;

        /// The exit status, or nothing while the child is still running. A child killed by a
        /// signal reports the negated signal number, as in Python.
        std::optional<int> returncode() const;

        /// @}

    protected:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // POPEN_H
