#ifndef POPEN_P_H
#define POPEN_P_H

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
        std::map<std::string, std::string> env;

        // std
        IODev stdin_dev;
        IODev stdout_dev;
        IODev stderr_dev;

        FILE *stdin_file = nullptr;
        FILE *stdout_file = nullptr;
        FILE *stderr_file = nullptr;

        bool text = false;
        bool close_fds = true;
        int pipesize = -1;

#ifdef _WIN32
        const StartupInfo *startupinfo = nullptr;
        int creationflags = 0;
#else
        std::function<void()> preexec_fn;
        bool restore_signals = true;
        bool start_new_session = false;
        std::vector<int> pass_fds;
        int group = 0;
        std::vector<int> extra_groups;

        // user
        struct user_info {
            bool has_value;
            union {
                int num;
                const char *str;
            };
        };
        user_info user = {false};

        int umask = -1;
        int process_group = -1;
#endif

    public:
        //
        // Data
        //
        bool _child_created = false;

        int pid = -1;
        std::optional<int> returncode;

        std::string _input;
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

        int _fork_exec(const std::set<int> &fds_to_keep, char **envs, int p2cread, int p2cwrite,
                       int c2pread, int c2pwrite, int errread, int errwrite, int errpipe_read,
                       int errpipe_write, int gid, const std::vector<int> &gids, int uid,
                       bool allow_vfork);

        bool _handle_exitstatus(int status);

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

#endif // POPEN_P_H