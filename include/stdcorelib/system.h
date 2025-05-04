#ifndef STDCORELIB_SYSTEM_H
#define STDCORELIB_SYSTEM_H

#include <string>
#include <vector>
#include <filesystem>
#include <map>

#include <stdcorelib/stdc_global.h>
#include <stdcorelib/adt/array_view.h>

namespace stdc {

    namespace system {

        STDCORELIB_EXPORT std::filesystem::path application_path();
        STDCORELIB_EXPORT std::filesystem::path application_directory();
        STDCORELIB_EXPORT std::filesystem::path application_filename();
        STDCORELIB_EXPORT std::string application_name();
        STDCORELIB_EXPORT array_view<std::string> command_line_arguments();

        STDCORELIB_EXPORT std::vector<std::string> split_command_line(const std::string_view &command);
        STDCORELIB_EXPORT std::string join_command_line(const std::vector<std::string> &args);

        static std::map<std::string, std::string> environment();

    }

}

#endif // STDCORELIB_SYSTEM_H