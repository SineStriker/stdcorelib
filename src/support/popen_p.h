#ifndef POPEN_P_H
#define POPEN_P_H

#include <shared_mutex>
#include <tuple>
#include <cstdint>
#include <cstddef>
#include <stdexcept>

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

        static inline int handle_to_fd(Handle handle) {
#ifdef _WIN32
            return (intptr_t) handle;
#else
            return (int) handle;
#endif
        }

        static inline Handle fd_to_handle(int fd) {
#ifdef _WIN32
            return (Handle) (intptr_t) fd;
#else
            return handle;
#endif
        }

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
        bool close_fds = false;
        int pipesize = -1;
        int process_group = 0;

#ifdef _WIN32
        const STARTUPINFO *startupinfo = nullptr;
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
#endif

    public:
        //
        // Data
        //
        bool _child_created = false;

        int pid = -1;
        std::optional<int> returncode;

        std::shared_mutex _waitpid_lock;
        std::string _input;
        bool _communication_started = false;

        // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L895C14-L895C38
        double _sigint_wait_secs = 0.25;
        bool _closed_child_pipe_fds = false;

        Handle _devnull = InvalidHandle;

#ifdef _WIN32
        Handle _handle = InvalidHandle;
#endif

        std::error_code error_code;

    public:
        //
        // Methods
        //
        void done();
        void close_std_files();

        Handle _get_devnull();

        std::tuple<Handle, Handle, Handle, Handle, Handle, Handle> _get_handles();

        void _close_pipe_fds(Handle p2cread, Handle p2cwrite, Handle c2pread, Handle c2pwrite,
                             Handle errread, Handle errwrite);

        void _close_pipe_fds_1(Handle p2cread, Handle p2cwrite, Handle c2pread, Handle c2pwrite,
                               Handle errread, Handle errwrite);

#ifdef _WIN32
        void _execute_child(Handle p2cread, Handle p2cwrite, Handle c2pread, Handle c2pwrite,
                            Handle errread, Handle errwrite);
#else
        void _execute_child(Handle p2cread, Handle p2cwrite, Handle c2pread, Handle c2pwrite,
                            Handle errread, Handle errwrite, int gid, const std::vector<int> &gids,
                            int uid);
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