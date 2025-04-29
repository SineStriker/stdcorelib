#include "popen.h"
#include "popen_p.h"

#include <unistd.h>

#include <csignal>

namespace stdc {

    static inline std::error_code make_last_error_code() {
        return std::error_code(errno, std::system_category());
    }

    // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L1348
    bool Popen::Impl::_get_handles(int &p2cread, int &p2cwrite, int &c2pread, int &c2pwrite,
                                   int &errread, int &errwrite) {
        if (stdin_dev.kind == 0 && stdout_dev.kind == 0 && stderr_dev.kind == 0) {
            return true;
        }

        int p2cread = -1, p2cwrite = -1;
        int c2pread = -1, c2pwrite = -1;
        int errread = -1, errwrite = -1;

        // TODO

        return true;
    }

    // TODO

    bool Popen::Impl::_internal_poll() {
        // TODO
        return {};
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
        if (kill(pid, sig) == 0) {
            return true;
        }
        error_code = make_last_error_code();
        return false;
    }


}