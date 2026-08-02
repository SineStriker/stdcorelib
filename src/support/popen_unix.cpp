// SPDX-License-Identifier: MIT

#include "popen.h"
#include "popen_p.h"

#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <grp.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <csignal>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

#include "str.h"
#include "scope_guard.h"

namespace stdc {

    static inline std::error_code make_last_error_code() {
        return std::error_code(errno, std::system_category());
    }

    static void set_cloexec(int fd, bool on) {
        int flags = fcntl(fd, F_GETFD);
        if (flags == -1) {
            return;
        }
        int wanted = on ? (flags | FD_CLOEXEC) : (flags & ~FD_CLOEXEC);
        if (wanted != flags) {
            fcntl(fd, F_SETFD, wanted);
        }
    }

    static bool make_pipe(int &read_fd, int &write_fd) {
        int fds[2];
#ifdef __linux__
        if (pipe2(fds, O_CLOEXEC) != 0) {
            return false;
        }
#else
        if (pipe(fds) != 0) {
            return false;
        }
        set_cloexec(fds[0], true);
        set_cloexec(fds[1], true);
#endif
        read_fd = fds[0];
        write_fd = fds[1];
        return true;
    }

    void Popen::Impl::_reap() {
        // Nothing to release here: waitpid() has already reaped the child.
    }

    void Popen::Impl::_cleanup() {
        close_std_files();
        _reap();
    }

    // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L272
    //
    // Children nobody is going to wait for. Until someone does, each one sits in the process
    // table as a zombie, so the list is swept whenever a new child is started. That is what
    // Python's _active list does, and it costs nothing while no process is being spawned.
    static std::mutex &abandoned_mutex() {
        static std::mutex mtx;
        return mtx;
    }

    static std::vector<pid_t> &abandoned_pids() {
        static std::vector<pid_t> pids;
        return pids;
    }

    static void reap_abandoned() {
        std::lock_guard<std::mutex> lock(abandoned_mutex());
        auto &pids = abandoned_pids();
        pids.erase(std::remove_if(pids.begin(), pids.end(),
                                  [](pid_t child) {
                                      int status;
                                      pid_t ret = waitpid(child, &status, WNOHANG);
                                      // Gone, or not ours to collect. Either way, stop looking.
                                      return ret == child || (ret == -1 && errno != EINTR);
                                  }),
                   pids.end());
    }

    void Popen::Impl::_release_child() {
        std::lock_guard<std::mutex> lock(abandoned_mutex());
        abandoned_pids().push_back(pid_t(pid));
    }

    bool Popen::Impl::_get_devnull() {
        int devnull = open("/dev/null", O_RDWR | O_CLOEXEC);
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

        // descriptors we opened ourselves, to be closed if a later step fails
        std::array<int, 10> err_close_fds;
        int err_close_fds_cnt = 0;
        auto err_close_fd_guard = make_scope_guard([&]() {
            for (int i = 0; i < err_close_fds_cnt; i++) {
                close(err_close_fds[i]);
            }
            if (_devnull != InvalidHandle) {
                close(_devnull);
                _devnull = InvalidHandle;
            }
        });
        const auto &push_err_close_fd = [&](int fd) { err_close_fds[err_close_fds_cnt++] = fd; };

        // create a pipe
        const auto &create_pipe = [this](int &read_fd, int &write_fd) {
            if (!make_pipe(read_fd, write_fd)) {
                error_code = make_last_error_code();
                error_api = "pipe";
                return false;
            }
            return true;
        };

        // open or return devnull
        const auto &open_devnull = [this](int &fd) {
            if (_devnull == InvalidHandle && !_get_devnull()) {
                return false;
            }
            fd = _devnull;
            return true;
        };

        // take a descriptor from the caller, which stays theirs to close
        const auto &convert_from_fd = [this](int &target, int fd) {
            if (fd == -1) {
                error_code = std::make_error_code(std::errc::bad_file_descriptor);
                error_api = "fileno";
                return false;
            }
            target = fd;
            return true;
        };

        //
        // transaction start
        //

        // stdin
        switch (stdin_dev.kind) {
            case IODev::None:
                break;
            case IODev::Builtin: {
                switch (stdin_dev.data.builtin) {
                    case PIPE: {
                        if (!create_pipe(p2cread, p2cwrite)) {
                            return false;
                        }
                        push_err_close_fd(p2cread);
                        push_err_close_fd(p2cwrite);
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
                        push_err_close_fd(c2pread);
                        push_err_close_fd(c2pwrite);
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
                        push_err_close_fd(errread);
                        push_err_close_fd(errwrite);
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

        err_close_fd_guard.dismiss();
        return true;
    }

    void Popen::Impl::_close_pipe_fds(Handle p2cread, int p2cwrite, int c2pread, Handle c2pwrite,
                                      int errread, Handle errwrite) {
        _close_pipe_fds_1(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);
        _closed_child_pipe_fds = true;
    }

    void Popen::Impl::_close_pipe_fds_1(Handle p2cread, int p2cwrite, int c2pread, Handle c2pwrite,
                                        int errread, Handle errwrite) {
        // Unlike Windows, nothing here was duplicated, so a descriptor the caller handed us is
        // still theirs. Close an end only when both ends are set, which is true of the pipes we
        // made and of nothing else.
        if (p2cread != -1 && p2cwrite != -1 && p2cread != _devnull) {
            close(p2cread);
        }
        if (c2pwrite != -1 && c2pread != -1 && c2pwrite != _devnull) {
            close(c2pwrite);
        }
        if (errwrite != -1 && errread != -1 && errwrite != _devnull) {
            close(errwrite);
        }
        if (_devnull != InvalidHandle) {
            close(_devnull);
            _devnull = InvalidHandle;
        }
    }

    struct Popen::Impl::ChildArgs {
        // null terminated arrays, all owned by the caller
        char *const *exec_array;
        char *const *argv;
        char *const *envp; // null to keep our own environment

        const char *cwd; // null to stay put

        // ascending, and the child must not close these
        const int *fds_to_keep;
        size_t fds_to_keep_len;

        int p2cread, p2cwrite;
        int c2pread, c2pwrite;
        int errread, errwrite;
        int errpipe_read, errpipe_write;

        int gid, uid; // -1 to leave alone
        const int *extra_gids;
        int extra_gids_len; // 0 to leave alone
    };

    // https://github.com/python/cpython/blob/v3.13.3/Modules/_posixsubprocess.c#L575
    //
    // Closes every descriptor at or above start_fd except the ones to keep, which must be sorted.
    static void close_open_fds(int start_fd, const int *keep, size_t keep_len) {
        long open_max = sysconf(_SC_OPEN_MAX);
        if (open_max < 0 || open_max > 1 << 20) {
            open_max = 1 << 20;
        }
        size_t k = 0;
        for (int fd = start_fd; fd < int(open_max); ++fd) {
            while (k < keep_len && keep[k] < fd) {
                ++k;
            }
            if (k < keep_len && keep[k] == fd) {
                continue;
            }
            close(fd);
        }
    }

    static void write_all(int fd, const char *data, size_t size) {
        while (size > 0) {
            ssize_t n = write(fd, data, size);
            if (n <= 0) {
                if (n < 0 && errno == EINTR) {
                    continue;
                }
                return;
            }
            data += n;
            size -= size_t(n);
        }
    }

    static void write_str(int fd, const char *str) {
        write_all(fd, str, strlen(str));
    }

    /// Writes value as lowercase hex. snprintf is not async signal safe, this is.
    static void write_hex(int fd, int value) {
        char buf[sizeof(int) * 2 + 1];
        char *cur = buf + sizeof(buf);
        do {
            *--cur = "0123456789abcdef"[value % 16];
            value /= 16;
        } while (value != 0 && cur != buf);
        write_all(fd, cur, size_t(buf + sizeof(buf) - cur));
    }

    // https://github.com/python/cpython/blob/v3.13.3/Modules/_posixsubprocess.c#L664
    void Popen::Impl::_child_exec(const ChildArgs &ca) {
        // Tells the parent the failure happened before exec, so the message is not a bad path.
        const char *err_msg = "noexec";
        int first_exec_errno = 0;

        // Returns only on failure, with errno set.
        const auto &run = [&]() {
            for (size_t i = 0; i < ca.fds_to_keep_len; ++i) {
                // errpipe_write is in this list but must stay close-on-exec. Its closing is what
                // tells the parent that exec succeeded.
                if (ca.fds_to_keep[i] != ca.errpipe_write) {
                    set_cloexec(ca.fds_to_keep[i], false);
                }
            }

            // close the parent's ends
            if (ca.p2cwrite != -1) {
                close(ca.p2cwrite);
            }
            if (ca.c2pread != -1) {
                close(ca.c2pread);
            }
            if (ca.errread != -1) {
                close(ca.errread);
            }
            close(ca.errpipe_read);

            // A child end that already sits on 0, 1 or 2 would be overwritten by a later dup2.
            int c2pwrite = ca.c2pwrite;
            int errwrite = ca.errwrite;
            if (c2pwrite == 0) {
                c2pwrite = dup(c2pwrite);
                if (c2pwrite < 0) {
                    return;
                }
                set_cloexec(c2pwrite, true);
            }
            while (errwrite == 0 || errwrite == 1) {
                errwrite = dup(errwrite);
                if (errwrite < 0) {
                    return;
                }
                set_cloexec(errwrite, true);
            }

            // dup2 clears FD_CLOEXEC, but it is a no-op when the two are equal, so clear it here.
            if (ca.p2cread == 0) {
                set_cloexec(0, false);
            } else if (ca.p2cread != -1 && dup2(ca.p2cread, 0) < 0) {
                return;
            }
            if (c2pwrite == 1) {
                set_cloexec(1, false);
            } else if (c2pwrite != -1 && dup2(c2pwrite, 1) < 0) {
                return;
            }
            if (errwrite == 2) {
                set_cloexec(2, false);
            } else if (errwrite != -1 && dup2(errwrite, 2) < 0) {
                return;
            }

            if (ca.cwd) {
                if (chdir(ca.cwd) == -1) {
                    err_msg = "noexec:chdir";
                    return;
                }
            }

            if (umask >= 0) {
                ::umask(mode_t(umask));
            }

            if (restore_signals) {
                // What CPython's _Py_RestoreSignals() undoes.
                signal(SIGPIPE, SIG_DFL);
                signal(SIGXFSZ, SIG_DFL);
            }

            if (start_new_session && setsid() == -1) {
                return;
            }
            if (process_group >= 0 && setpgid(0, process_group) == -1) {
                return;
            }
            if (ca.extra_gids_len > 0 &&
                setgroups(size_t(ca.extra_gids_len),
                          reinterpret_cast<const gid_t *>(ca.extra_gids)) == -1) {
                return;
            }
            if (ca.gid != -1 && setregid(gid_t(ca.gid), gid_t(ca.gid)) == -1) {
                return;
            }
            if (ca.uid != -1 && setreuid(uid_t(ca.uid), uid_t(ca.uid)) == -1) {
                return;
            }

            err_msg = "";
            if (preexec_fn) {
                // This is where the user has asked us to deadlock their program.
                preexec_fn();
            }

            // After preexec_fn, which may have opened descriptors of its own.
            if (close_fds) {
                close_open_fds(3, ca.fds_to_keep, ca.fds_to_keep_len);
            }

            // The parent built the candidate list from PATH, so this is the search.
            for (int i = 0; ca.exec_array[i]; ++i) {
                if (ca.envp) {
                    execve(ca.exec_array[i], ca.argv, ca.envp);
                } else {
                    execv(ca.exec_array[i], ca.argv);
                }
                if (errno != ENOENT && errno != ENOTDIR && first_exec_errno == 0) {
                    first_exec_errno = errno;
                }
            }
        };
        run();

        // Report the first exec error rather than the last.
        int saved_errno = first_exec_errno ? first_exec_errno : errno;
        if (saved_errno) {
            write_str(ca.errpipe_write, "OSError:");
            write_hex(ca.errpipe_write, saved_errno);
            write_str(ca.errpipe_write, ":");
        } else {
            write_str(ca.errpipe_write, "SubprocessError:0:");
        }
        // strerror is not async signal safe. The parent looks the number up instead.
        write_str(ca.errpipe_write, err_msg);
    }

    int Popen::Impl::_fork_exec(const ChildArgs &ca) {
        pid_t child = fork();
        if (child == 0) {
            _child_exec(ca);
            _exit(255);
        }
        return int(child);
    }

    /// The directories PATH names, or the standard ones when it says nothing.
    static std::vector<std::string>
        exec_search_path(const std::map<std::string, std::string> &env) {
        std::string path;
        auto it = env.find("PATH");
        if (it != env.end()) {
            path = it->second;
        } else if (const char *p = getenv("PATH")) {
            path = p;
        } else {
            path = "/bin:/usr/bin";
        }

        std::vector<std::string> dirs;
        size_t start = 0;
        while (start <= path.size()) {
            size_t end = path.find(':', start);
            if (end == std::string::npos) {
                end = path.size();
            }
            // An empty entry means the working directory. Skipping it is what a shell's secure
            // PATH does, and searching it here would be a surprise.
            if (end > start) {
                dirs.push_back(path.substr(start, end - start));
            }
            start = end + 1;
        }
        return dirs;
    }

    // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1449
    bool Popen::Impl::_execute_child(int p2cread, int p2cwrite, int c2pread, int c2pwrite,
                                     int errread, int errwrite, int gid,
                                     const std::vector<int> &gids, int uid) {
        assert(!args.empty());

        // Starting a child is the moment to collect the ones nobody is waiting for.
        reap_abandoned();

        if (shell) {
            // /bin/sh, not bash, is the one unix guarantees.
            const char *prefix_cmd[] = {"/bin/sh", "-c"};
            args.insert(args.begin(), std::begin(prefix_cmd), std::end(prefix_cmd));
            if (!executable.empty()) {
                args[0] = executable.string();
            }
        }
        if (executable.empty()) {
            executable = args[0];
        }

        // Candidate paths to try in order. A name with no slash is looked up along PATH, which is
        // what execvp would do, except that we cannot call it once the environment is replaced.
        std::vector<std::string> exec_paths;
        {
            std::string name = executable.string();
            if (name.find('/') != std::string::npos) {
                exec_paths.push_back(name);
            } else {
                for (const auto &dir : exec_search_path(env)) {
                    exec_paths.push_back(dir + "/" + name);
                }
            }
        }
        if (exec_paths.empty()) {
            error_code = std::make_error_code(std::errc::no_such_file_or_directory);
            error_msg = formatN("cannot find executable: %1", executable.string());
            return false;
        }

        std::vector<char *> exec_array;
        for (auto &path : exec_paths) {
            exec_array.push_back(path.data());
        }
        exec_array.push_back(nullptr);

        std::vector<char *> argv;
        for (auto &arg : args) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        // Built here rather than in the child, where allocating is not allowed.
        std::vector<std::string> env_items;
        std::vector<char *> envp;
        if (!env.empty()) {
            for (const auto &pair : env) {
                if (pair.first.find('=') != std::string::npos) {
                    error_code = std::make_error_code(std::errc::invalid_argument);
                    error_msg = formatN("illegal environment variable name: %1", pair.first);
                    return false;
                }
                env_items.push_back(pair.first + "=" + pair.second);
            }
            for (auto &item : env_items) {
                envp.push_back(item.data());
            }
            envp.push_back(nullptr);
        }

        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1862
        //
        // For transferring possible exec failure from child to parent.
        // Data format: "exception name:hex errno:description"
        int errpipe_read = -1, errpipe_write = -1;
        if (!make_pipe(errpipe_read, errpipe_write)) {
            error_code = make_last_error_code();
            error_api = "pipe";
            return false;
        }

        {
            // errpipe_write must not be in the standard io 0, 1, or 2 fd range.
            std::vector<int> low_fds;
            auto close_low_fds_guard = make_scope_guard([&] {
                for (int fd : low_fds) {
                    close(fd);
                }
            });
            while (errpipe_write < 3) {
                low_fds.push_back(errpipe_write);
                errpipe_write = dup(errpipe_write);
                if (errpipe_write == -1) {
                    close(errpipe_read);
                    error_code = make_last_error_code();
                    error_api = "dup";
                    return false;
                }
                set_cloexec(errpipe_write, true);
            }
        }

        std::vector<int> fds_to_keep = pass_fds;
        fds_to_keep.push_back(errpipe_write);
        std::sort(fds_to_keep.begin(), fds_to_keep.end());
        fds_to_keep.erase(std::unique(fds_to_keep.begin(), fds_to_keep.end()), fds_to_keep.end());

        std::string cwd_str = cwd.string();

        ChildArgs ca{};
        ca.exec_array = exec_array.data();
        ca.argv = argv.data();
        ca.envp = envp.empty() ? nullptr : envp.data();
        ca.cwd = cwd.empty() ? nullptr : cwd_str.c_str();
        ca.fds_to_keep = fds_to_keep.data();
        ca.fds_to_keep_len = fds_to_keep.size();
        ca.p2cread = p2cread, ca.p2cwrite = p2cwrite;
        ca.c2pread = c2pread, ca.c2pwrite = c2pwrite;
        ca.errread = errread, ca.errwrite = errwrite;
        ca.errpipe_read = errpipe_read, ca.errpipe_write = errpipe_write;
        ca.gid = gid;
        ca.uid = uid;
        ca.extra_gids = gids.data();
        ca.extra_gids_len = int(gids.size());

        std::string errpipe_data;

        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1873
        {
            int tmp_pid = _fork_exec(ca);
            if (tmp_pid == -1) {
                auto err = make_last_error_code();
                close(errpipe_read);
                close(errpipe_write);
                error_code = err;
                error_api = "fork";
                return false;
            }
            pid = tmp_pid;
            _child_created = true;
            close(errpipe_write);

            _close_pipe_fds(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);

            // Wait for exec to fail or succeed. The write end is open only in the child, so the
            // read goes to end of file as soon as exec replaces it.
            while (true) {
                char buf[4096];
                ssize_t n = read(errpipe_read, buf, sizeof(buf));
                if (n < 0 && errno == EINTR) {
                    continue;
                }
                if (n <= 0) {
                    break;
                }
                errpipe_data.append(buf, size_t(n));
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

        // The child died on its own, so reap it before reporting.
        int status;
        pid_t ret_pid;
        do {
            ret_pid = waitpid(pid, &status, 0);
        } while (ret_pid == -1 && errno == EINTR);
        if (ret_pid == pid) {
            _handle_exitstatus(status);
        } else {
            returncode = std::numeric_limits<int>::max();
        }

        // "exception name:hex errno:description"
        auto first = errpipe_data.find(':');
        auto second = first == std::string::npos ? first : errpipe_data.find(':', first + 1);
        if (second == std::string::npos) {
            error_code = std::make_error_code(std::errc::bad_message);
            error_msg = formatN("bad exception data from child: %1", errpipe_data);
            return false;
        }

        auto hex_errno = errpipe_data.substr(first + 1, second - first - 1);
        auto detail = errpipe_data.substr(second + 1);
        int err_val = int(std::strtol(hex_errno.c_str(), nullptr, 16));
        if (err_val == 0) {
            error_code = std::make_error_code(std::errc::bad_message);
            error_msg = detail.empty() ? "child failed before exec" : detail;
            return false;
        }

        error_code = std::error_code(err_val, std::system_category());
        // "noexec:chdir" names the working directory, anything else names the program.
        error_msg = formatN("%1: %2", detail == "noexec:chdir" ? cwd_str : executable.string(),
                            error_code.message());
        return false;
    }

    void Popen::Impl::_handle_exitstatus(int status) {
        if (WIFSTOPPED(status)) {
            returncode = -WSTOPSIG(status);
        } else if (WIFSIGNALED(status)) {
            returncode = -WTERMSIG(status);
        } else {
            returncode = WEXITSTATUS(status);
        }
    }

    bool Popen::Impl::_internal_poll() {
        error_code.clear();

        if (returncode) {
            return true;
        }
        if (!_child_created) {
            error_code = std::make_error_code(std::errc::no_such_process);
            return false;
        }

        // Two waitpid calls at once would race for the status, and only one of them could win.
        std::unique_lock<std::shared_mutex> lock(_waitpid_lock, std::try_to_lock);
        if (!lock.owns_lock()) {
            return false;
        }
        if (returncode) {
            return true;
        }

        int status;
        pid_t ret = waitpid(pid, &status, WNOHANG);
        if (ret == pid) {
            _handle_exitstatus(status);
            return true;
        }
        if (ret == 0) {
            // Still running, which is not an error. The caller tells the two apart by whether
            // returncode() is set.
            return false;
        }
        if (errno == EINTR) {
            return false;
        }
        if (errno == ECHILD) {
            // Waiting has been disabled for this process, so the status is gone for good. Python
            // reports 0 rather than leaving the caller with nothing.
            returncode = 0;
            return true;
        }
        error_code = make_last_error_code();
        return false;
    }

    bool Popen::Impl::_wait(int timeout) {
        error_code.clear();

        if (returncode) {
            return true;
        }
        if (!_child_created) {
            error_code = std::make_error_code(std::errc::no_such_process);
            return false;
        }

        if (timeout < 0) {
            while (!returncode) {
                std::unique_lock<std::shared_mutex> lock(_waitpid_lock);
                if (returncode) {
                    break;
                }
                int status;
                pid_t ret = waitpid(pid, &status, 0);
                if (ret == pid) {
                    _handle_exitstatus(status);
                    break;
                }
                if (ret == -1) {
                    if (errno == EINTR) {
                        continue;
                    }
                    if (errno == ECHILD) {
                        returncode = 0;
                        break;
                    }
                    error_code = make_last_error_code();
                    return false;
                }
                // waitpid has been known to return 0 without WNOHANG, see bpo-14396.
            }
            return true;
        }

        // waitpid has no deadline, so poll with a delay that grows to 50 ms, as Python does.
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        auto delay = std::chrono::microseconds(500);
        while (true) {
            if (_internal_poll()) {
                return true;
            }
            if (error_code.value() != 0) {
                return false;
            }
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return false;
            }
            auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now);
            delay = std::min({delay * 2, remaining, std::chrono::microseconds(50000)});
            std::this_thread::sleep_for(delay);
        }
    }

    bool Popen::Impl::kill_impl() {
        return send_signal_impl(SIGKILL);
    }

    bool Popen::Impl::terminate_impl() {
        return send_signal_impl(SIGTERM);
    }

    // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L2218
    bool Popen::Impl::send_signal_impl(int sig) {
        error_code.clear();

        // Polling first narrows the window in which the pid has been recycled and the signal
        // would land on somebody else's process.
        if (!returncode) {
            std::ignore = _internal_poll();
            error_code.clear();
        }
        if (returncode) {
            return true;
        }
        if (!_child_created) {
            error_code = std::make_error_code(std::errc::no_such_process);
            return false;
        }

        if (::kill(pid, sig) == 0) {
            return true;
        }
        if (errno == ESRCH) {
            // It went away between the poll and the kill, which is not a failure.
            return true;
        }
        error_code = make_last_error_code();
        return false;
    }

}
