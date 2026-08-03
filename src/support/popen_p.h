// SPDX-License-Identifier: MIT

#ifndef STDCORELIB_POPEN_P_H
#define STDCORELIB_POPEN_P_H

#include <tuple>
#include <cstdint>
#include <set>
#include <shared_mutex>

#include <stdcorelib/support/popen.h>

namespace stdc {

    class Popen::Impl {
    public:
#ifdef _WIN32
        using Handle = void *; // windows HANDLE
        static inline const Handle InvalidHandle = (Handle) (intptr_t) -1;
#else
        using Handle = int; // file descriptor
        static inline const Handle InvalidHandle = -1;
#endif

        Impl();
        ~Impl();

        //
        // input
        //
        std::filesystem::path executable;
        std::vector<std::string> args;
        bool shell = false;

        std::filesystem::path cwd;
        // Empty and absent mean different things: an empty map asks for an empty environment,
        // no map at all hands down ours.
        std::optional<std::map<std::string, std::string>> env;

        // std
        IODev stdin_dev;
        IODev stdout_dev;
        IODev stderr_dev;

        // Mutable because the accessors are const. Reading a child's output does not change the
        // Popen.
        mutable Stream stdin_stream;
        mutable Stream stdout_stream;
        mutable Stream stderr_stream;

        bool text = false;
        bool close_fds = true;
        bool detached = false;
        int pipesize = -1;

#ifdef _WIN32
        const StartupInfo *startupinfo = nullptr;
        int creationflags = 0;
#else
        std::function<void()> preexec_fn;
        bool restore_signals = true;
        bool start_new_session = false;
        std::vector<int> pass_fds;
        int group = -1;
        std::vector<int> extra_groups;

        // user
        struct user_info {
            bool has_value = false;
            bool is_name = false;
            int num = -1;
            std::string str;
        };
        user_info user;

        int umask = -1;
        int process_group = -1;
#endif

    public:
        //
        // Data
        //
        bool _child_created = false;
        bool _detached_started = false;

        int pid = -1;
        std::optional<int> returncode;

        bool _communication_started = false;

        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L894
        double _sigint_wait_secs = 0.25;
        bool _closed_child_pipe_fds = false;

        Handle _devnull = InvalidHandle; // will be closed after creating child

#ifdef _WIN32
        Handle _handle = InvalidHandle;
        int tid = -1;
#else
        std::shared_mutex _waitpid_lock;
#endif

        // error data during start
        std::string error_msg;
        const char *error_api = nullptr;
        std::error_code error_code;

    public:
        //
        // Methods
        //
        bool done();
        void close_std_files();

        /// Releases the OS process handle once the exit status is known. Separate from _cleanup()
        /// so that waiting for a child leaves its pipes open for reading.
        void _reap();

        void _cleanup();

        bool _get_devnull();
        bool _get_handles(Handle &p2cread, Handle &p2cwrite, Handle &c2pread, Handle &c2pwrite,
                          Handle &errread, Handle &errwrite);

        void _close_pipe_fds(Handle p2cread, int p2cwrite, int c2pread, Handle c2pwrite,
                             int errread, Handle errwrite);

        // https://github.com/python/cpython/blob/v3.13.3/Lib/subprocess.py#L1050
        // close but not set _closed_child_pipe_fds, why?
        void _close_pipe_fds_1(Handle p2cread, int p2cwrite, int c2pread, Handle c2pwrite,
                               int errread, Handle errwrite);

#ifdef _WIN32
        bool _execute_child(Handle p2cread, int p2cwrite, int c2pread, Handle c2pwrite, int errread,
                            Handle errwrite);
#else
        bool _execute_child(int p2cread, int p2cwrite, int c2pread, int c2pwrite, int errread,
                            int errwrite, int gid, const std::vector<int> &gids, int uid);

        /// Everything the child needs, packed so that the code after fork() only reads plain
        /// memory. Defined in popen_unix.cpp.
        struct ChildArgs;

        /// Forks and runs _child_exec() in the child. Detached mode returns the second child's
        /// pid after waiting for its launcher. Returns -1 for an initial fork failure and 0 for
        /// a detached launcher failure reported through the exec error pipe.
        int _fork_exec(const ChildArgs &ca);

        /// Runs in the forked child and never returns. Only async signal safe calls belong here.
        void _child_exec(const ChildArgs &ca);

        void _handle_exitstatus(int status);

#endif
        bool _internal_poll();
        bool _wait(int timeout = -1);

        bool kill_impl();
        bool terminate_impl();

        bool send_signal_impl(int sig);
        std::tuple<std::string, std::string> communicate_impl(const std::string &input = {},
                                                              int timeout = -1);
    };

}

#endif // STDCORELIB_POPEN_P_H
