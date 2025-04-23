#ifndef STDCORELIB_SYSTEM_H
#define STDCORELIB_SYSTEM_H

#include <string>
#include <vector>
#include <filesystem>

#include <stdcorelib/global.h>

namespace stdc {

    namespace system {

        STDCORELIB_EXPORT std::filesystem::path application_path();
        STDCORELIB_EXPORT std::filesystem::path application_directory();
        STDCORELIB_EXPORT std::filesystem::path application_filename();
        STDCORELIB_EXPORT std::string application_name();
        STDCORELIB_EXPORT std::vector<std::string> command_line_arguments();
        STDCORELIB_EXPORT std::vector<std::string> split_command_line(const std::string &command);

    }

}

#endif // STDCORELIB_SYSTEM_H