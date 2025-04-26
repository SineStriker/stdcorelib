#ifndef STDCORELIB_EXPERIMENTAL_PROCESS_H
#define STDCORELIB_EXPERIMENTAL_PROCESS_H

#include <string>
#include <vector>
#include <filesystem>

#include <stdcorelib/stdc_global.h>

namespace stdc::experimental {

    class STDCORELIB_EXPORT Process {
    public:
        /**
         * @brief Execute a process and wait for it to finish.
         *
         * @param command     Path to the executable file.
         * @param args        Arguments to pass to the executable.
         * @param cwd         Working directory for the child process.
         * @param stdoutFile  Child process stdout output, "-" to redirect to stdout.
         * @param stderrFile  Child process stderr output, "-" to redirect to stderr.
         *
         * @return int      Exit code of the child process.
         */
        static int start(const std::filesystem::path &command, const std::vector<std::string> &args,
                         const std::filesystem::path &cwd, const std::string &stdoutFile = {},
                         const std::string &stderrFile = {});

        /**
         * @brief Check the output of a process.
         *
         * @param command   Path to the executable file.
         * @param args      Arguments to pass to the executable.
         * @param cwd       Working directory for the child process.
         * @param output    Output string to capture the child output.
         *
         * @return int      Exit code of the child process.
         */
        static int checkOptput(const std::filesystem::path &command,
                               const std::vector<std::string> &args,
                               const std::filesystem::path &cwd, std::string &output);
    };

}

#endif // STDCORELIB_EXPERIMENTAL_PROCESS_H