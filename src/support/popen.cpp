// SPDX-License-Identifier: MIT

#include "popen.h"
#include "popen_p.h"

#include <fcntl.h>

#ifdef _WIN32
#  include <io.h>
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
            if (detached) {
                _release_child();
            } else {
                std::ignore = kill_impl();
                std::ignore = _wait();
            }
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

    bool Popen::Impl::done() {
        if (_child_created) {
            return true;
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
                struct passwd *pw = getpwnam(user.str);
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

        // convert to file descriptors
        int p2cwrite = -1, c2pread = -1, errread = -1;
        if (p2cwrite_h != InvalidHandle) {
            p2cwrite = _open_osfhandle((intptr_t) p2cwrite_h, 0);
        }
        if (c2pread_h != InvalidHandle) {
            c2pread = _open_osfhandle((intptr_t) c2pread_h, 0);
        }
        if (errread_h != InvalidHandle) {
            errread = _open_osfhandle((intptr_t) errread_h, 0);
        }
#else
        Handle p2cread = InvalidHandle, p2cwrite = InvalidHandle;
        Handle c2pread = InvalidHandle, c2pwrite = InvalidHandle;
        Handle errread = InvalidHandle, errwrite = InvalidHandle;
        if (!_get_handles(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite)) {
            return false;
        }
#endif
        // open C File objects
        if (p2cwrite != -1) {
            stdin_stream.open(Popen_fdopen(p2cwrite, text ? "w" : "wb"));
        }
        if (c2pread != -1) {
            stdout_stream.open(Popen_fdopen(c2pread, text ? "r" : "rb"));
        }
        if (errread != -1) {
            stderr_stream.open(Popen_fdopen(errread, text ? "r" : "rb"));
        }

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
        STDCORELIB_DISABLE_COPY_MOVE(sigpipe_guard)
    };

    // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1862
    //
    // A pipe blocks its writer once full, so stdout and stderr cannot be drained after the child
    // exits, nor one after the other. Python gives each pipe a reader thread on Windows. So do
    // we, on both platforms.
    std::tuple<std::string, std::string> Popen::Impl::communicate_impl(const std::string &input,
                                                                       int timeout) {
        error_code.clear();

        if (!_child_created) {
            error_code = std::make_error_code(std::errc::no_such_process);
            return {};
        }

        const auto &read_all = [](FILE *file, std::string &dest) {
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof(buf), file)) > 0) {
                dest.append(buf, n);
            }
        };

        std::string out, err;
        std::thread out_thread, err_thread;
        if (stdout_stream.is_open()) {
            out_thread = std::thread(read_all, stdout_stream.file(), std::ref(out));
        }
        if (stderr_stream.is_open()) {
            err_thread = std::thread(read_all, stderr_stream.file(), std::ref(err));
        }

        // Write the input and close the pipe. Closing is the only thing that tells a child
        // reading to end of input that there is no more coming.
        if (stdin_stream.is_open()) {
            sigpipe_guard guard;
            if (!input.empty()) {
                stdin_stream.write(input.data(), std::streamsize(input.size()));
            }
            stdin_stream.flush();
            stdin_stream.close();
        }
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

        if (out_thread.joinable()) {
            out_thread.join();
        }
        if (err_thread.joinable()) {
            err_thread.join();
        }
        close_std_files();
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

    Popen &Popen::env(const std::map<std::string, std::string> &env) {
        stdc_impl_t;
        impl.env = env;
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
        info.str = user;
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