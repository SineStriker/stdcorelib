// SPDX-License-Identifier: MIT

#include "popen.h"
#include "popen_p.h"

#include <fcntl.h>

#ifdef _WIN32
#  include <io.h>
#  include "stdc_windows.h"
#else
#  include <csignal>
#  include <pwd.h>
#  include <unistd.h>
#endif

#include <thread>

#include "pimpl.h"
#include "str.h"

namespace stdc {

    /// A streambuf over a FILE *. MSVC offers this as basic_filebuf(FILE *) and libstdc++ as
    /// __gnu_cxx::stdio_filebuf. Neither is portable, so we write it once here.
    ///
    /// Building on stdio rather than the OS handle keeps the text mode translation that
    /// _fdopen already set up, and leaves buffering to the C library.
    class Popen::Stream::Buf : public std::streambuf {
    public:
        ~Buf() override {
            close();
        }

        void open(FILE *file) {
            close();
            _file = file;
            setg(_buf, _buf, _buf);
        }

        bool is_open() const {
            return _file != nullptr;
        }

        FILE *file() const {
            return _file;
        }

        void close() {
            if (!_file) {
                return;
            }
            std::fflush(_file);
            std::fclose(_file);
            _file = nullptr;
            setg(_buf, _buf, _buf);
        }

    protected:
        int_type underflow() override {
            if (!_file) {
                return traits_type::eof();
            }
            if (gptr() < egptr()) {
                return traits_type::to_int_type(*gptr());
            }
            size_t n = std::fread(_buf, 1, sizeof(_buf), _file);
            if (n == 0) {
                return traits_type::eof();
            }
            setg(_buf, _buf, _buf + n);
            return traits_type::to_int_type(*gptr());
        }

        std::streamsize xsputn(const char *s, std::streamsize n) override {
            if (!_file) {
                return 0;
            }
            return std::streamsize(std::fwrite(s, 1, size_t(n), _file));
        }

        int_type overflow(int_type c) override {
            if (!_file) {
                return traits_type::eof();
            }
            if (c != traits_type::eof()) {
                auto ch = traits_type::to_char_type(c);
                if (std::fwrite(&ch, 1, 1, _file) != 1) {
                    return traits_type::eof();
                }
            }
            return c;
        }

        int sync() override {
            return (_file && std::fflush(_file) == 0) ? 0 : -1;
        }

    private:
        FILE *_file = nullptr;
        char _buf[4096]{};
    };

    // Base classes are built before members, so the buffer does not exist yet. Pass null and
    // point the base at it with rdbuf() once it does.
    Popen::Stream::Stream() : std::iostream(nullptr), _buf(new Buf()) {
        rdbuf(_buf.get());
    }

    Popen::Stream::~Stream() = default;

    void Popen::Stream::open(FILE *file) {
        _buf->open(file);
        clear();
    }

    void Popen::Stream::close() {
        _buf->close();
    }

    bool Popen::Stream::is_open() const {
        return _buf->is_open();
    }

    FILE *Popen::Stream::file() const {
        return _buf->file();
    }

    Popen::Impl::Impl() = default;

    Popen::Impl::~Impl() {
        if (_child_created && !returncode) {
            std::ignore = kill_impl();
            std::ignore = _wait();
        }
        _cleanup();
    }

    static FILE *Popen_fdopen(int fd, const char *modes) {
#ifdef _WIN32
        return _fdopen(fd, modes);
#else
        return fdopen(fd, modes);
#endif
    }

    static int Popen_close_fd(int fd) {
#ifdef _WIN32
        return _close(fd);
#else
        return close(fd);
#endif
    }

    bool Popen::Impl::done() {
        error_code.clear();
        error_msg.clear();
        error_api = nullptr;

        if (_child_created || _detached_started) {
            return true;
        }
        pid = -1;
        returncode.reset();
        _closed_child_pipe_fds = false;

        const auto is_pipe = [](const IODev &dev) {
            return dev.kind == IODev::Builtin && dev.data.builtin == IOType::PIPE;
        };
        if (detached && (is_pipe(stdin_dev) || is_pipe(stdout_dev) || is_pipe(stderr_dev))) {
            error_code = std::make_error_code(std::errc::invalid_argument);
            error_msg = "PIPE is not supported for a detached process";
            return false;
        }

        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L847
        if (stdout_dev.kind == 1 && stdout_dev.data.builtin == IOType::STDOUT) {
            error_code = std::make_error_code(std::errc::invalid_argument);
            error_msg = "STDOUT can only be used for stderr";
            return false;
        }

        // ###FIXME: do we need to check?
        if (stdin_dev.kind == 1 && stdin_dev.data.builtin == IOType::STDOUT) {
            error_code = std::make_error_code(std::errc::invalid_argument);
            error_msg = "STDOUT can only be used for stderr";
            return false;
        }

#ifndef _WIN32
        if (!pass_fds.empty() && !close_fds) {
            fprintf(stderr, "stdc::Popen: %s\n", "pass_fds overriding close_fds.");
            close_fds = true;
        }
#endif

        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L881
        // We don't need to handle string encodings in C++.

#ifndef _WIN32
        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L911
        //
        // -1 means "leave it alone", which is also what setregid() and setreuid() take.
        int gid = group;
        std::vector<int> gids = extra_groups;
        int uid = -1;
        if (user.has_value) {
            if (user.is_name) {
                errno = 0;
                struct passwd *pw = getpwnam(user.str.c_str());
                if (!pw) {
                    error_code = errno ? std::error_code(errno, std::system_category())
                                       : std::make_error_code(std::errc::invalid_argument);
                    error_msg = formatN("no such user: %1", user.str);
                    return false;
                }
                uid = int(pw->pw_uid);
            } else {
                uid = user.num;
            }
        }
#endif

        // Input and output objects. The general principle is like
        // this:
        //
        // Parent                   Child
        // ------                   -----
        // p2cwrite   ---stdin--->  p2cread
        // c2pread    <--stdout---  c2pwrite
        // errread    <--stderr---  errwrite
        //
        // On POSIX, the child objects are file descriptors.  On
        // Windows, these are Windows file handles.  The parent objects
        // are file descriptors on both platforms.  The parent objects
        // are -1 when not using PIPEs. The child objects are -1
        // when not redirecting.
        //
        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1003
#ifdef _WIN32
        Handle p2cread = InvalidHandle, p2cwrite_h = InvalidHandle;
        Handle c2pread_h = InvalidHandle, c2pwrite = InvalidHandle;
        Handle errread_h = InvalidHandle, errwrite = InvalidHandle;
        if (!_get_handles(p2cread, p2cwrite_h, c2pread_h, c2pwrite, errread_h, errwrite)) {
            return false;
        }

        // Convert the parent's handles to CRT descriptors. _open_osfhandle takes ownership only
        // on success, so keep the raw handles until each conversion has completed.
        int p2cwrite = -1, c2pread = -1, errread = -1;
        const int binary_or_text = text ? _O_TEXT : _O_BINARY;
        if (p2cwrite_h != InvalidHandle) {
            p2cwrite = _open_osfhandle((intptr_t) p2cwrite_h, _O_WRONLY | binary_or_text);
            if (p2cwrite != -1)
                p2cwrite_h = InvalidHandle;
        }
        if (c2pread_h != InvalidHandle) {
            c2pread = _open_osfhandle((intptr_t) c2pread_h, _O_RDONLY | binary_or_text);
            if (c2pread != -1)
                c2pread_h = InvalidHandle;
        }
        if (errread_h != InvalidHandle) {
            errread = _open_osfhandle((intptr_t) errread_h, _O_RDONLY | binary_or_text);
            if (errread != -1)
                errread_h = InvalidHandle;
        }
        if ((p2cwrite_h != InvalidHandle && p2cwrite == -1) ||
            (c2pread_h != InvalidHandle && c2pread == -1) ||
            (errread_h != InvalidHandle && errread == -1)) {
            error_code = errno ? std::error_code(errno, std::generic_category())
                               : std::make_error_code(std::errc::bad_file_descriptor);
            error_api = "_open_osfhandle";
            if (p2cwrite != -1)
                _close(p2cwrite);
            if (c2pread != -1)
                _close(c2pread);
            if (errread != -1)
                _close(errread);
            if (p2cwrite_h != InvalidHandle)
                ::CloseHandle(p2cwrite_h);
            if (c2pread_h != InvalidHandle)
                ::CloseHandle(c2pread_h);
            if (errread_h != InvalidHandle)
                ::CloseHandle(errread_h);
            _close_pipe_fds_1(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);
            return false;
        }
#else
        Handle p2cread = InvalidHandle, p2cwrite = InvalidHandle;
        Handle c2pread = InvalidHandle, c2pwrite = InvalidHandle;
        Handle errread = InvalidHandle, errwrite = InvalidHandle;
        if (!_get_handles(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite)) {
            return false;
        }
#endif
        // Turn all descriptors into FILE objects transactionally. A failed fdopen leaves its
        // descriptor owned by the caller, so every partial result needs explicit cleanup.
        FILE *stdin_file = p2cwrite == -1 ? nullptr : Popen_fdopen(p2cwrite, text ? "w" : "wb");
        FILE *stdout_file = c2pread == -1 ? nullptr : Popen_fdopen(c2pread, text ? "r" : "rb");
        FILE *stderr_file = errread == -1 ? nullptr : Popen_fdopen(errread, text ? "r" : "rb");
        if ((p2cwrite != -1 && !stdin_file) || (c2pread != -1 && !stdout_file) ||
            (errread != -1 && !stderr_file)) {
            error_code = errno ? std::error_code(errno, std::generic_category())
                               : std::make_error_code(std::errc::io_error);
            error_api = "fdopen";
            if (stdin_file)
                std::fclose(stdin_file);
            else if (p2cwrite != -1)
                Popen_close_fd(p2cwrite);
            if (stdout_file)
                std::fclose(stdout_file);
            else if (c2pread != -1)
                Popen_close_fd(c2pread);
            if (stderr_file)
                std::fclose(stderr_file);
            else if (errread != -1)
                Popen_close_fd(errread);
            _close_pipe_fds_1(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);
            return false;
        }
        if (stdin_file)
            stdin_stream.open(stdin_file);
        if (stdout_file)
            stdout_stream.open(stdout_file);
        if (stderr_file)
            stderr_stream.open(stderr_file);

#ifdef _WIN32
        bool result = _execute_child(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);
#else
        bool result = _execute_child(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite, //
                                     gid, gids, uid);
#endif

        if (!result) {
            // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1049
            close_std_files();
            if (!_closed_child_pipe_fds) {
                _close_pipe_fds_1(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);
            }
            // A POSIX exec failure briefly created and then reaped a child. Externally start()
            // still failed, so restore the pre-start state and allow a corrected retry.
            if (returncode) {
                _child_created = false;
            }
        }
        return result;
    }

    void Popen::Impl::close_std_files() {
        stdout_stream.close();
        stderr_stream.close();
        stdin_stream.close();
    }

    /// Blocks SIGPIPE for its lifetime. Writing to a pipe whose reader has gone raises it, and its
    /// default action ends the process. Nothing to do on Windows.
    class sigpipe_guard {
    public:
#ifdef _WIN32
        sigpipe_guard() = default;
#else
        sigpipe_guard() {
            sigset_t block;
            sigemptyset(&block);
            sigaddset(&block, SIGPIPE);
            pthread_sigmask(SIG_BLOCK, &block, &_old);
            _was_blocked = sigismember(&_old, SIGPIPE) == 1;
        }

        ~sigpipe_guard() {
            if (!_was_blocked) {
                // Take the signal we caused off the queue, or unblocking would deliver it.
#  ifndef __APPLE__
                sigset_t pending;
                sigemptyset(&pending);
                sigaddset(&pending, SIGPIPE);
                struct timespec zero = {0, 0};
                while (sigtimedwait(&pending, nullptr, &zero) >= 0) {
                }
#  endif
            }
            pthread_sigmask(SIG_SETMASK, &_old, nullptr);
        }

    private:
        sigset_t _old{};
        bool _was_blocked = false;
#endif
        STDC_DISABLE_COPY_MOVE(sigpipe_guard)
    };

    /// Runs \a body on a worker thread and leaves whatever it throws in \a error for the thread
    /// that joins it, since an exception crossing a thread boundary would call std::terminate.
    template <class F>
    void run_capturing(std::exception_ptr &error, F &&body) {
#ifdef STDC_EXCEPTIONS
        try {
            body();
        } catch (...) {
            error = std::current_exception();
        }
#else
        // Nothing can be thrown here, so nothing arrives to be reported and the slot stays empty.
        (void) error;
        body();
#endif
    }

    // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1862
    //
    // A pipe blocks its writer once full, so stdout and stderr cannot be drained after the child
    // exits, nor one after the other. Python gives each pipe a reader thread on Windows. So do
    // we, on both platforms.
    std::tuple<std::string, std::string> Popen::Impl::communicate_impl(const std::string &input,
                                                                       int timeout) {
        error_code.clear();

        // Same answer as the other five, rather than the no_such_process the check below would
        // give. A detached child exists, it is just not ours to talk to.
        if (_detached_started) {
            error_code = std::make_error_code(std::errc::operation_not_supported);
            return {};
        }

        if (!_child_created) {
            error_code = std::make_error_code(std::errc::no_such_process);
            return {};
        }

        std::string out, err;
        std::thread out_thread, err_thread, in_thread;
        std::exception_ptr out_error, err_error, in_error;

        const auto &read_all = [](FILE *file, std::string &dest, std::exception_ptr &error) {
            run_capturing(error, [&] {
                char buf[4096];
                size_t n;
                while ((n = std::fread(buf, 1, sizeof(buf), file)) > 0) {
                    dest.append(buf, n);
                }
            });
        };

        const auto &start_workers = [&] {
            if (stdout_stream.is_open()) {
                out_thread =
                    std::thread(read_all, stdout_stream.file(), std::ref(out), std::ref(out_error));
            }
            if (stderr_stream.is_open()) {
                err_thread =
                    std::thread(read_all, stderr_stream.file(), std::ref(err), std::ref(err_error));
            }

            // Input has its own worker too. Otherwise a child that never reads can fill the pipe
            // and block this thread before it ever reaches the timeout below.
            if (stdin_stream.is_open()) {
                in_thread = std::thread([&] {
                    run_capturing(in_error, [&] {
                        sigpipe_guard guard;
                        if (!input.empty()) {
                            stdin_stream.write(input.data(), std::streamsize(input.size()));
                        }
                        stdin_stream.flush();
                        stdin_stream.close();
                    });
                });
            }
        };

#ifdef STDC_EXCEPTIONS
        try {
            start_workers();
        } catch (...) {
            // No writer exists yet when thread construction can fail, so closing stdin is safe.
            stdin_stream.close();
            std::ignore = kill_impl();
            std::ignore = _wait();
            if (out_thread.joinable())
                out_thread.join();
            if (err_thread.joinable())
                err_thread.join();
            close_std_files();
            throw;
        }
#else
        // Starting a thread can still fail, but without exceptions it takes the process down
        // where it happens rather than arriving here to be cleaned up after.
        start_workers();
#endif
        _communication_started = true;

        // A timeout kills the child rather than leaving it behind. Its exit is what closes the
        // write ends, and without that the reader threads below never finish.
        if (!_wait(timeout)) {
            auto wait_error = error_code;
            std::ignore = kill_impl();
            std::ignore = _wait();
            error_code =
                wait_error.value() != 0 ? wait_error : std::make_error_code(std::errc::timed_out);
        }

        if (in_thread.joinable()) {
            in_thread.join();
        }
        if (out_thread.joinable()) {
            out_thread.join();
        }
        if (err_thread.joinable()) {
            err_thread.join();
        }
        close_std_files();

        if (in_error)
            std::rethrow_exception(in_error);
        if (out_error)
            std::rethrow_exception(out_error);
        if (err_error)
            std::rethrow_exception(err_error);
        return {out, err};
    }

}

namespace stdc {

    Popen::Popen() : _impl(new Impl()) {
    }

    Popen::~Popen() = default;

    Popen::Popen(Popen &&RHS) noexcept {
        std::swap(_impl, RHS._impl);
    }

    Popen &Popen::operator=(Popen &&RHS) noexcept {
        std::swap(_impl, RHS._impl);
        return *this;
    }

    Popen &Popen::executable(const std::filesystem::path &executable) {
        stdc_impl_t;
        impl.executable = executable;
        return *this;
    }

    Popen &Popen::args(const std::vector<std::string> &args) {
        stdc_impl_t;
        impl.args = args;
        return *this;
    }

    Popen &Popen::shell(bool shell) {
        stdc_impl_t;
        impl.shell = shell;
        return *this;
    }

    Popen &Popen::cwd(const std::filesystem::path &cwd) {
        stdc_impl_t;
        impl.cwd = cwd;
        return *this;
    }

    Popen &Popen::env(const std::optional<std::map<std::string, std::string>> &env) {
        stdc_impl_t;
        impl.env = env;
        return *this;
    }

    Popen &Popen::env(std::initializer_list<std::pair<const std::string, std::string>> env) {
        stdc_impl_t;
        impl.env = std::map<std::string, std::string>(env);
        return *this;
    }

    Popen &Popen::stdin_(IODev dev) {
        stdc_impl_t;
        impl.stdin_dev = dev;
        return *this;
    }

    Popen &Popen::stdout_(IODev dev) {
        stdc_impl_t;
        impl.stdout_dev = dev;
        return *this;
    }

    Popen &Popen::stderr_(IODev dev) {
        stdc_impl_t;
        impl.stderr_dev = dev;
        return *this;
    }

    Popen &Popen::text(bool text) {
        stdc_impl_t;
        impl.text = text;
        return *this;
    }

    Popen &Popen::close_fds(bool close_fds) {
        stdc_impl_t;
        impl.close_fds = close_fds;
        return *this;
    }

    Popen &Popen::detached(bool detached) {
        stdc_impl_t;
        if (!impl._child_created && !impl._detached_started)
            impl.detached = detached;
        return *this;
    }

    Popen &Popen::pipesize(int pipesize) {
        stdc_impl_t;
        impl.pipesize = pipesize;
        return *this;
    }

#ifdef _WIN32
    Popen &Popen::startupinfo(const StartupInfo *startupinfo) {
        stdc_impl_t;
        impl.startupinfo = startupinfo;
        return *this;
    }

    Popen &Popen::creationflags(int creationflags) {
        stdc_impl_t;
        impl.creationflags = creationflags;
        return *this;
    }
#else
    Popen &Popen::preexec_fn(const std::function<void()> &preexec_fn) {
        stdc_impl_t;
        impl.preexec_fn = preexec_fn;
        return *this;
    }

    Popen &Popen::restore_signals(bool restore_signals) {
        stdc_impl_t;
        impl.restore_signals = restore_signals;
        return *this;
    }

    Popen &Popen::start_new_session(bool start_new_session) {
        stdc_impl_t;
        impl.start_new_session = start_new_session;
        return *this;
    }

    Popen &Popen::pass_fds(const std::vector<int> &pass_fds) {
        stdc_impl_t;
        impl.pass_fds = pass_fds;
        return *this;
    }

    Popen &Popen::group(int group) {
        stdc_impl_t;
        impl.group = group;
        return *this;
    }

    Popen &Popen::extra_groups(const std::vector<int> &extra_groups) {
        stdc_impl_t;
        impl.extra_groups = extra_groups;
        return *this;
    }

    Popen &Popen::user(int user) {
        auto &info = _impl->user;
        info.has_value = true;
        info.is_name = false;
        info.num = user;
        return *this;
    }

    Popen &Popen::user(const char *user) {
        auto &info = _impl->user;
        info.has_value = true;
        info.is_name = true;
        info.str = user ? user : "";
        return *this;
    }

    Popen &Popen::umask(int umask) {
        stdc_impl_t;
        impl.umask = umask;
        return *this;
    }

    Popen &Popen::process_group(int process_group) {
        stdc_impl_t;
        impl.process_group = process_group;
        return *this;
    }
#endif

    bool Popen::start(std::string *err_msg) {
        stdc_impl_t;

        bool result = impl.done();
        if (result) {
            return true;
        }

        // system api error
        if (err_msg) {
            if (impl.error_api) {
                *err_msg = formatN("%1: %2", impl.error_api, impl.error_code.message());
                return false;
            }

            // invalid argument, file no found, etc.
            if (!impl.error_msg.empty()) {
                *err_msg = impl.error_msg;
                return false;
            }

            if (impl.error_code.value() != 0) {
                *err_msg = impl.error_code.message();
                return false;
            }

            // unknown error
            *err_msg = "unknown error";
        }
        return false;
    }

    std::error_code Popen::error_code() const {
        stdc_impl_t;
        return impl.error_code;
    }

    bool Popen::poll() {
        stdc_impl_t;
        return impl._internal_poll();
    }

    bool Popen::wait(int timeout) {
        stdc_impl_t;
        // we don't wait for the next Ctrl+C like python
        return impl._wait(timeout);
    }

    std::tuple<std::string, std::string> Popen::communicate(const std::string &input, int timeout) {
        stdc_impl_t;
        return impl.communicate_impl(input, timeout);
    }

    bool Popen::send_signal(int sig) {
        stdc_impl_t;
        return impl.send_signal_impl(sig);
    }

    bool Popen::terminate() {
        stdc_impl_t;
        return impl.terminate_impl();
    }

    bool Popen::kill() {
        stdc_impl_t;
        return impl.kill_impl();
    }

    const std::filesystem::path &Popen::executable() const {
        stdc_impl_t;
        return impl.executable;
    }

    array_view<std::string> Popen::args() const {
        stdc_impl_t;
        return impl.args;
    }

    Popen::Stream &Popen::stdin_() const {
        stdc_impl_t;
        return impl.stdin_stream;
    }

    Popen::Stream &Popen::stdout_() const {
        stdc_impl_t;
        return impl.stdout_stream;
    }

    Popen::Stream &Popen::stderr_() const {
        stdc_impl_t;
        return impl.stderr_stream;
    }

    int Popen::pid() const {
        stdc_impl_t;
        return impl.pid;
    }

    bool Popen::detached() const {
        stdc_impl_t;
        return impl.detached;
    }

    std::optional<int> Popen::returncode() const {
        stdc_impl_t;
        return impl.returncode;
    }

}
