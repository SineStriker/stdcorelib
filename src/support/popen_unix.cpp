#include "popen.h"
#include "popen_p.h"

#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

#include <csignal>
#include <cassert>
#include <array>

#include "str.h"
#include "scope_guard.h"

#include "3rdparty/llvm/smallvector.h"

namespace stdc {

    static inline std::error_code make_last_error_code() {
        return std::error_code(errno, std::system_category());
    }

    bool Popen::Impl::_get_devnull() {
        // Create a null device handle
        int devnull = open("/dev/null", O_RDWR);
        if (devnull == -1) {
            error_code = make_last_error_code();
            error_api = "open";
            return false;
        }
        _devnull = devnull;
        return true;
    }

    // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1348
    bool Popen::Impl::_get_handles(int &p2cread, int &p2cwrite, int &c2pread, int &c2pwrite,
                                   int &errread, int &errwrite) {
        if (stdin_dev.kind == 0 && stdout_dev.kind == 0 && stderr_dev.kind == 0) {
            return true;
        }

        p2cread = -1, p2cwrite = -1;
        c2pread = -1, c2pwrite = -1;
        errread = -1, errwrite = -1;

        // handles that should be closed if error occurs
        std::array<int, 10> err_close_handles;
        int err_close_handles_cnt = 0;
        auto err_close_handle_guard = make_scope_guard([&]() {
            for (int i = 0; i < err_close_handles_cnt; i++) {
                close(err_close_handles[i]);
            }
            if (_devnull != InvalidHandle) {
                close(_devnull);
                _devnull = InvalidHandle;
            }
        });
        const auto &push_err_close_handle = [&](int handle) {
            err_close_handles[err_close_handles_cnt++] = handle;
        };

        // handles that should be closed anyway
        const auto &push_dup_close_handle = push_err_close_handle;

        // create a pipe
        const auto &create_pipe = [this](int &read_handle, int &write_handle) {
            int pipes[2];
            if (pipe(pipes) != 0) {
                error_code = make_last_error_code();
                error_api = "pipe";
                return false;
            }
            return true;
        };

        // open or return devnull
        const auto &open_devnull = [this](int &handle) {
            if (_devnull == InvalidHandle && !_get_devnull()) {
                return false;
            }
            handle = _devnull;
            return true;
        };

        // convert a file descriptor to a handle
        const auto &convert_from_fd = [this](int &handle, int fd) {
            if (fd == -1) {
                error_code = std::make_error_code(std::errc::bad_file_descriptor);
                error_api = "fileno";
                return false;
            }
            handle = fd;
            return true;
        };

        //
        // transaction start
        //

        switch (stdin_dev.kind) {
            case IODev::None:
                break;
            case IODev::Builtin: {
                switch (stdin_dev.data.builtin) {
                    case PIPE: {
                        if (!create_pipe(p2cread, p2cwrite)) {
                            return false;
                        }
                        push_dup_close_handle(p2cread);
                        push_err_close_handle(p2cwrite);
#ifdef F_SETPIPE_SZ
                        if (pipesize > 0) {
                            fcntl(p2cwrite, F_SETPIPE_SZ, pipesize);
                        }
#endif
                        break;
                    };
                    case DEVNULL: {
                        if (!open_devnull(p2cread)) {
                            return false;
                        }
                        break;
                    };
                    default: {
                        error_code = std::make_error_code(std::errc::invalid_argument);
                        error_msg = formatN("invalid stdin type: %1", int(stdin_dev.data.builtin));
                        return false;
                    }
                }
                break;
            }
            case IODev::FD: {
                if (!convert_from_fd(p2cread, stdin_dev.data.fd)) {
                    return false;
                }
                break;
            }
            case IODev::CFile: {
                if (!convert_from_fd(p2cread, fileno(stdin_dev.data.file))) {
                    return false;
                }
                break;
            }
            default:
                break;
        }
        push_err_close_handle(p2cread);

        // stdout
        switch (stdout_dev.kind) {
            case IODev::None: {
                break;
            }
            case IODev::Builtin: {
                switch (stdout_dev.data.builtin) {
                    case PIPE: {
                        if (!create_pipe(c2pread, c2pwrite)) {
                            return false;
                        }
                        push_err_close_handle(c2pread);
                        push_dup_close_handle(c2pwrite);
#ifdef F_SETPIPE_SZ
                        if (pipesize > 0) {
                            fcntl(c2pwrite, F_SETPIPE_SZ, pipesize);
                        }
#endif
                        break;
                    };
                    case DEVNULL: {
                        if (!open_devnull(c2pwrite)) {
                            return false;
                        }
                        break;
                    };
                    default: {
                        error_code = std::make_error_code(std::errc::invalid_argument);
                        error_msg =
                            formatN("invalid stdout type: %1", int(stdout_dev.data.builtin));
                        return false;
                    }
                }
                break;
            }
            case IODev::FD: {
                if (!convert_from_fd(c2pwrite, stdout_dev.data.fd)) {
                    return false;
                }
                break;
            }
            case IODev::CFile: {
                if (!convert_from_fd(c2pwrite, fileno(stdout_dev.data.file))) {
                    return false;
                }
                break;
            }
            default:
                break;
        }
        push_err_close_handle(c2pwrite);

        // stderr
        switch (stderr_dev.kind) {
            case IODev::None: {
                break;
            }
            case IODev::Builtin: {
                switch (stderr_dev.data.builtin) {
                    case PIPE: {
                        if (!create_pipe(errread, errwrite)) {
                            return false;
                        }
                        push_err_close_handle(errread);
                        push_dup_close_handle(errwrite);
#ifdef F_SETPIPE_SZ
                        if (pipesize > 0) {
                            fcntl(errwrite, F_SETPIPE_SZ, pipesize);
                        }
#endif
                        break;
                    };
                    case DEVNULL: {
                        if (!open_devnull(errwrite)) {
                            return false;
                        }
                        break;
                    };
                    case STDOUT: {
                        if (c2pwrite != -1) {
                            errwrite = c2pwrite;
                        } else if (!convert_from_fd(errwrite, fileno(stdout))) {
                            return false;
                        }
                        break;
                    };
                }
                break;
            }
            case IODev::FD: {
                if (!convert_from_fd(errwrite, stderr_dev.data.fd)) {
                    return false;
                }
                break;
            }
            case IODev::CFile: {
                if (!convert_from_fd(errwrite, fileno(stderr_dev.data.file))) {
                    return false;
                }
                break;
            }
            default:
                break;
        }

        //
        // transaction end
        //

        // release guard
        err_close_handle_guard.dismiss();
        return true;
    }

    void Popen::Impl::_close_pipe_fds(Handle p2cread, int p2cwrite, int c2pread, Handle c2pwrite,
                                      int errread, Handle errwrite) {
        _close_pipe_fds_1(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);
        _closed_child_pipe_fds = true;
    }

    void Popen::Impl::_close_pipe_fds_1(Handle p2cread, int p2cwrite, int c2pread, Handle c2pwrite,
                                        int errread, Handle errwrite) {
        (void) p2cwrite;
        (void) c2pread;
        (void) errread;

        if (p2cread != InvalidHandle) {
            close(p2cread);
        }
        if (c2pwrite != InvalidHandle) {
            close(c2pwrite);
        }
        if (errwrite != InvalidHandle) {
            close(errwrite);
        }
        if (_devnull != InvalidHandle) {
            close(_devnull);
            _devnull = InvalidHandle;
        }
    }

    // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1449
    bool Popen::Impl::_execute_child(int p2cread, int p2cwrite, int c2pread, int c2pwrite,
                                     int errread, int errwrite, int gid,
                                     const std::vector<int> &gids, int uid) {
        assert(!args.empty());

        if (shell) {
            const auto unix_shell = "/bin/bash";
            const char *prefix_cmd[] = {
                unix_shell,
                "-c",
            };
            args.insert(args.begin(), std::begin(prefix_cmd), std::end(prefix_cmd));
        }

        if (executable.empty()) {
            executable = args[0];
        }

        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1862
        //
        // For transferring possible exec failure from child to parent.
        // Data format: "exception name:hex errno:description"
        int errpipe[2];
        if (pipe(errpipe) != 0) {
            error_code = make_last_error_code();
            error_api = "pipe";
            return false;
        }

        auto &errpipe_read = errpipe[0];
        auto &errpipe_write = errpipe[1];
        {
            llvm::SmallVector<int> low_fds_to_close;
            const auto &close_low_fds_guard = make_scope_guard([&] {
                for (int fd : std::as_const(low_fds_to_close)) {
                    close(fd);
                }
            });

            // errpipe_write must not be in the standard io 0, 1, or 2 fd range.
            while (errpipe_write < 3) {
                low_fds_to_close.push_back(errpipe_write);
                errpipe_write = dup(errpipe_write);
                if (errpipe_write == -1) {
                    close(errpipe_read);
                    error_code = make_last_error_code();
                    error_api = "dup";
                    return false;
                }
            }
        }

        char **envs = nullptr;
        const auto &delete_envs_guard = make_scope_guard([envs] {
            if (!envs) {
                return;
            }
            for (char *item = envs[0]; item; item++) {
                delete[] item;
            }
            delete[] envs;
        });

        if (!env.empty()) {
            envs = new char *[env.size() + 1];
            int i = 0;
            for (const auto &pair : env) {
                auto item = new char[pair.first.size() + 1 + pair.second.size() + 1];
                strncpy(item, pair.first.c_str(), pair.first.size());
                item[pair.first.size()] = '=';
                strncpy(item + pair.first.size() + 1, pair.second.c_str(), pair.second.size());
                item[pair.first.size() + 1 + pair.second.size()] = '\0';
                envs[i++] = item;
            }
            envs[i] = nullptr;
        }

        std::set<int> fds_to_keep(pass_fds.begin(), pass_fds.end());
        fds_to_keep.insert(errpipe_write);

        std::string errpipe_data;

        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1873
        {
            int tmp_pid = _fork_exec(fds_to_keep, envs, p2cread, p2cwrite, c2pread, c2pwrite,
                                     errread, errwrite, gid, gids, uid, true);
            if (tmp_pid == -1) {
                close(errpipe_read);
                close(errpipe_write);

                error_code = make_last_error_code();
                error_api = "fork";
                return false;
            }
            pid = tmp_pid;
            _child_created = true;
            close(errpipe_write);

            _close_pipe_fds(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);

            // Wait for exec to fail or succeed; possibly raising an
            // exception (limited in size)
            while (true) {
                char buf[4096];
                int errpipe_data_size = read(errread, buf, 50000);
                if (errpipe_data_size <= 0)
                    break;
                errpipe_data.append(buf, errpipe_data_size);
                if (errpipe_data.size() > 50000) {
                    break;
                }
            }
            close(errpipe_read);
        }

        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1930
        if (errpipe_data.empty()) {
            return true;
        }

        int status;
        pid_t ret_pid = waitpid(pid, &status, 0);
        if (ret_pid == pid) {
            if (!_handle_exitstatus(status)) {
                return false;
            }
        } else {
            returncode = std::numeric_limits<int>::max();
        }

        int err_val = std::atoi(errpipe_data.c_str());
        if (err_val == 0) {
            error_code = std::make_error_code(std::errc::bad_message);
            error_msg = formatN("bad exception data from child: %1", errpipe_data);
            return false;
        }
        error_code = std::error_code(err_val, std::system_category());
        return false;
    }

    int Popen::Impl::_fork_exec(const std::set<int> &fds_to_keep, char **envs, int p2cread,
                                int p2cwrite, int c2pread, int c2pwrite, int errread, int errwrite,
                                int gid, const std::vector<int> &gids, int uid, bool allow_vfork) {
        return 0;
    }

    bool Popen::Impl::_handle_exitstatus(int status) {
        if (WIFEXITED(status)) {
            returncode = WEXITSTATUS(status);
            return true;
        }
        if (WIFSIGNALED(status)) {
            returncode = -WTERMSIG(status);
            return true;
        }
        if (WIFSTOPPED(status)) {
            returncode = WSTOPSIG(status);
            return true;
        }
        return false;
    }

    // TODO

    bool Popen::Impl::_internal_poll() {
        int pip[2];
        auto result = pipe(pip); // scope, initializer_list
        // TODO
        return false;
    }

    bool Popen::Impl::_wait(int timeout) {
        // TODO
        return {};
    }

    bool Popen::Impl::kill_impl() {
        return send_signal_impl(SIGKILL);
    }

    bool Popen::Impl::terminate_impl() {
        return send_signal_impl(SIGTERM);
    }

    bool Popen::Impl::send_signal_impl(int sig) {
        if (!_internal_poll()) {
            return false;
        }
        if (returncode) {
            return true;
        }
        error_code.clear();
        if (::kill(pid, sig) == 0) {
            return true;
        }
        error_code = make_last_error_code();
        return false;
    }

    std::tuple<std::string, std::string> Popen::Impl::communicate_impl(const std::string &input,
                                                                       int timeout) {
        // TODO
        return {};
    }


}