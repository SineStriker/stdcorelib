#include "popen.h"
#include "popen_p.h"

namespace stdc {

    // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L1348
    std::tuple<Popen::Impl::FD, Popen::Impl::FD, Popen::Impl::FD, Popen::Impl::FD, Popen::Impl::FD,
               Popen::Impl::FD>
        Popen::Impl::get_handles() {
        if (!stdin_stream && !stdout_type && !stdout_stream && !stderr_type && !stderr_stream) {
            return {-1, -1, -1, -1, -1, -1};
        }

        int p2cread = -1, p2cwrite = -1;
        int c2pread = -1, c2pwrite = -1;
        int errread = -1, errwrite = -1;

        // TODO

        return {p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite};
    }

}