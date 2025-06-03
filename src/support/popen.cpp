#include "popen.h"
#include "popen_p.h"

#include <fcntl.h>

#ifdef _WIN32
#  include <io.h>
#else
#  include <sys/io.h>
#endif

#include "pimpl.h"
#include "str.h"

namespace stdc {

    Popen::Impl::Impl() = default;

    Popen::Impl::~Impl() {
        if (_child_created && !returncode) {
            // ###FIXME: we cannot run detached process now.
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
        int gid = -1;
        std::vector<int> gids;
        int uid = -1;
        // TODO: unix
        // ...
        int i = 0;
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
            stdin_file = Popen_fdopen(p2cwrite, text ? "w" : "wb");
        }
        if (c2pread != -1) {
            stdout_file = Popen_fdopen(c2pread, text ? "r" : "rb");
        }
        if (errread != -1) {
            stderr_file = Popen_fdopen(errread, text ? "r" : "rb");
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
        if (stdout_file) {
            fclose(stdout_file);
            stdin_file = nullptr;
        }
        if (stderr_file) {
            fclose(stderr_file);
            stdin_file = nullptr;
        }
        if (stdin_file) {
            fclose(stdin_file);
            stdin_file = nullptr;
        }
    }

    void Popen::Impl::_cleanup() {
        close_std_files();

        // TODO

        pid = -1;
    }

}

namespace stdc {

    /*!
        \class Popen
        \brief A class to create and control child processes.

        This class provides a way to create and control child processes. It is similar to
        Python's \c subprocess.Popen class.

        \sa https://docs.python.org/3/library/subprocess.html
    */

    /*!
        Constructs a Popen object.
     */
    Popen::Popen() : _impl(new Impl()) {
    }

    /*!
        Destroys the Popen object.
    */
    Popen::~Popen() = default;

    Popen::Popen(Popen &&RHS) noexcept {
        std::swap(_impl, RHS._impl);
    }

    Popen &Popen::operator=(Popen &&RHS) noexcept {
        std::swap(_impl, RHS._impl);
        return *this;
    }

    Popen &Popen::executable(const std::filesystem::path &executable) {
        __stdc_impl_t;
        impl.executable = executable;
        return *this;
    }

    Popen &Popen::args(const std::vector<std::string> &args) {
        __stdc_impl_t;
        impl.args = args;
        return *this;
    }

    Popen &Popen::shell(bool shell) {
        __stdc_impl_t;
        impl.shell = shell;
        return *this;
    }

    Popen &Popen::cwd(const std::filesystem::path &cwd) {
        __stdc_impl_t;
        impl.cwd = cwd;
        return *this;
    }

    Popen &Popen::env(const std::map<std::string, std::string> &env) {
        __stdc_impl_t;
        impl.env = env;
        return *this;
    }

    Popen &Popen::stdin_(IODev dev) {
        __stdc_impl_t;
        impl.stdin_dev = dev;
        return *this;
    }

    Popen &Popen::stdout_(IODev dev) {
        __stdc_impl_t;
        impl.stdout_dev = dev;
        return *this;
    }

    Popen &Popen::stderr_(IODev dev) {
        __stdc_impl_t;
        impl.stderr_dev = dev;
        return *this;
    }

    Popen &Popen::text(bool text) {
        __stdc_impl_t;
        impl.text = text;
        return *this;
    }

    Popen &Popen::close_fds(bool close_fds) {
        __stdc_impl_t;
        impl.close_fds = close_fds;
        return *this;
    }

    Popen &Popen::pipesize(int pipesize) {
        __stdc_impl_t;
        impl.pipesize = pipesize;
        return *this;
    }

#ifdef _WIN32
    Popen &Popen::startupinfo(const StartupInfo *startupinfo) {
        __stdc_impl_t;
        impl.startupinfo = startupinfo;
        return *this;
    }

    Popen &Popen::creationflags(int creationflags) {
        __stdc_impl_t;
        impl.creationflags = creationflags;
        return *this;
    }
#else
    Popen &Popen::preexec_fn(const std::function<void()> &preexec_fn) {
        __stdc_impl_t;
        impl.preexec_fn = preexec_fn;
        return *this;
    }

    Popen &Popen::restore_signals(bool restore_signals) {
        __stdc_impl_t;
        impl.restore_signals = restore_signals;
        return *this;
    }

    Popen &Popen::start_new_session(bool start_new_session) {
        __stdc_impl_t;
        impl.start_new_session = start_new_session;
        return *this;
    }

    Popen &Popen::pass_fds(const std::vector<int> &pass_fds) {
        __stdc_impl_t;
        impl.pass_fds = pass_fds;
        return *this;
    }

    Popen &Popen::group(int group) {
        __stdc_impl_t;
        impl.group = group;
        return *this;
    }

    Popen &Popen::extra_groups(const std::vector<int> &extra_groups) {
        __stdc_impl_t;
        impl.extra_groups = extra_groups;
        return *this;
    }

    Popen &Popen::user(int user) {
        auto &info = _impl->user;
        info.has_value = true;
        info.num = user;
        return *this;
    }

    Popen &Popen::user(const char *user) {
        auto &info = _impl->user;
        info.has_value = true;
        info.str = user;
        return *this;
    }

    Popen &Popen::umask(int umask) {
        __stdc_impl_t;
        impl.umask = umask;
        return *this;
    }

    Popen &Popen::process_group(int process_group) {
        __stdc_impl_t;
        impl.process_group = process_group;
        return *this;
    }
#endif

    /*!
        Starts the process. If a failure occurs, the error code will be set and the detailed error
        message will be stored into \a err_msg.
    */
    bool Popen::start(std::string *err_msg) {
        __stdc_impl_t;

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

    /*!
        Returns the error code of the last operation.
    */
    std::error_code Popen::error_code() const {
        __stdc_impl_t;
        return impl.error_code;
    }

    bool Popen::poll() {
        __stdc_impl_t;
        return impl._internal_poll();
    }

    bool Popen::wait(int timeout) {
        __stdc_impl_t;
        // we don't wait for the next Ctrl+C like python
        return impl._wait(timeout);
    }

    std::tuple<std::string, std::string> Popen::communicate(const std::string &input, int timeout) {
        __stdc_impl_t;
        return impl.communicate_impl(input, timeout);
    }

    bool Popen::send_signal(int sig) {
        __stdc_impl_t;
        return impl.send_signal_impl(sig);
    }

    bool Popen::terminate() {
        __stdc_impl_t;
        return impl.terminate_impl();
    }

    bool Popen::kill() {
        __stdc_impl_t;
        return impl.kill_impl();
    }

    const std::filesystem::path &Popen::executable() const {
        __stdc_impl_t;
        return impl.executable;
    }

    array_view<std::string> Popen::args() const {
        __stdc_impl_t;
        return impl.args;
    }

    FILE *Popen::stdin_() const {
        __stdc_impl_t;
        return impl.stdin_file;
    }

    FILE *Popen::stdout_() const {
        __stdc_impl_t;
        return impl.stdout_file;
    }

    FILE *Popen::stderr_() const {
        __stdc_impl_t;
        return impl.stderr_file;
    }

    int Popen::pid() const {
        __stdc_impl_t;
        return impl.pid;
    }

    std::optional<int> Popen::returncode() const {
        __stdc_impl_t;
        return impl.returncode;
    }

}