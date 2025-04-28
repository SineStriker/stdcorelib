#include "popen.h"
#include "popen_p.h"

#include <fcntl.h>
#include <io.h>

#include <algorithm>

#include "osapi_win.h"
#include "str.h"

#include "3rdparty/llvm/smallvector.h"

namespace fs = std::filesystem;

namespace stdc {

    constexpr UINT KillProcessExitCode = 0xf291;

    Popen::Impl::Handle Popen::Impl::_get_devnull() {
        if (_devnull) {
            return _devnull;
        }

        // Create a null device handle
        auto handle =
            CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            throw std::system_error(GetLastError(), std::system_category(), "CreateFileW");
        }
        _devnull = handle;
        return handle;
    }

    static HANDLE _make_inheritable(HANDLE handle) {
        HANDLE target_handle;
        if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &target_handle, 0, 1,
                             DUPLICATE_SAME_ACCESS)) {
            throw std::system_error(GetLastError(), std::system_category(), "DuplicateHandle");
        }
        return target_handle;
    }

    // Filter out console handles that can't be used
    // in lpAttributeList["handle_list"] and make sure the list
    // isn't empty. This also removes duplicate handles.
    template <class Container>
    static Container _filter_handle_list(const Container &handle_list) {
        // A handle with its lowest two bits set might be a special console
        // handle that if passed in lpAttributeList["handle_list"], will
        // cause it to fail.
        Container res;
        std::copy_if(
            handle_list.begin(), handle_list.end(), std::back_inserter(res), [](HANDLE handle) {
                return (((intptr_t) handle & 0x3) != 0x3 || GetFileType(handle) != FILE_TYPE_CHAR);
            });
        return res;
    }

    // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L1348
    std::tuple<void *, void *, void *, void *, void *, void *> Popen::Impl::_get_handles() {
        if (stdin_dev.kind == 0 && stdout_dev.kind == 0 && stderr_dev.kind == 0) {
            return {InvalidHandle, InvalidHandle, InvalidHandle,
                    InvalidHandle, InvalidHandle, InvalidHandle};
        }

        HANDLE p2cread = INVALID_HANDLE_VALUE, p2cwrite = INVALID_HANDLE_VALUE;
        HANDLE c2pread = INVALID_HANDLE_VALUE, c2pwrite = INVALID_HANDLE_VALUE;
        HANDLE errread = INVALID_HANDLE_VALUE, errwrite = INVALID_HANDLE_VALUE;

        struct ErrorCloseHandles {
            ErrorCloseHandles(Handle &devnull) : devnull(devnull) {
            }

            ~ErrorCloseHandles() {
                if (!ok) {
                    free();
                }
            }

            void free() {
                for (int i = 0; i < error_close_handles_count; i++) {
                    CloseHandle(error_close_handles[i]);
                }
                if (devnull) {
                    CloseHandle(devnull);
                }
            }

            void push(HANDLE handle) {
                error_close_handles[error_close_handles_count++] = handle;
            }

            bool ok = false;

            HANDLE error_close_handles[10];
            Handle &devnull;
            int error_close_handles_count = 0;
        };

        struct DuplicatedHandles {
        public:
            void push(HANDLE handle) {
                duplicated_close_handles[duplicated_close_handles_count++] = handle;
            }

            void free() {
                for (int i = 0; i < duplicated_close_handles_count; i++) {
                    CloseHandle(duplicated_close_handles[i]);
                }
            }

            HANDLE duplicated_close_handles[10];
            int duplicated_close_handles_count = 0;
        };

        ErrorCloseHandles err_close_handles(_devnull);
        DuplicatedHandles dup_handles;

        // stdin
        switch (stdin_dev.kind) {
            case IODev::Null: {
                p2cread = GetStdHandle(STD_INPUT_HANDLE);
                if (p2cread == INVALID_HANDLE_VALUE) {
                    HANDLE tmp_pipe;
                    if (!CreatePipe(&p2cread, &tmp_pipe, nullptr, 0)) {
                        throw std::system_error(GetLastError(), std::system_category(),
                                                "CreatePipe");
                    }
                    err_close_handles.push(p2cread);
                    dup_handles.push(p2cread);
                    CloseHandle(tmp_pipe);
                }
                break;
            }
            case IODev::Builtin: {
                switch (stdin_dev.data.builtin) {
                    case PIPE: {
                        if (!CreatePipe(&p2cread, &p2cwrite, nullptr, 0)) {
                            throw std::system_error(GetLastError(), std::system_category(),
                                                    "CreatePipe");
                        }
                        err_close_handles.push(p2cread);
                        err_close_handles.push(p2cwrite);
                        dup_handles.push(p2cread);
                        break;
                    };
                    case DEVNULL: {
                        p2cread = _get_devnull();
                        break;
                    };
                    default: {
                        throw std::invalid_argument(
                            formatN("invalid stdin type: %1", int(stdin_dev.data.builtin)));
                    }
                }
                break;
            }
            case IODev::FD: {
                p2cread = (HANDLE) _get_osfhandle(stdin_dev.data.fd);
                break;
            }
            case IODev::CFile: {
                p2cread = (HANDLE) _get_osfhandle(_fileno(stdin_dev.data.file));
                break;
            }
            default:
                break;
        }
        p2cread = _make_inheritable(p2cread);
        err_close_handles.push(p2cread);

        // stdout
        switch (stdout_dev.kind) {
            case IODev::Null: {
                c2pwrite = GetStdHandle(STD_OUTPUT_HANDLE);
                if (c2pwrite == INVALID_HANDLE_VALUE) {
                    HANDLE tmp_pipe;
                    if (!CreatePipe(&tmp_pipe, &c2pwrite, nullptr, 0)) {
                        throw std::system_error(GetLastError(), std::system_category(),
                                                "CreatePipe");
                    }
                    err_close_handles.push(c2pwrite);
                    dup_handles.push(c2pwrite);
                    CloseHandle(tmp_pipe);
                }
                break;
            }
            case IODev::Builtin: {
                switch (stdout_dev.data.builtin) {
                    case PIPE: {
                        if (!CreatePipe(&c2pread, &c2pwrite, nullptr, 0)) {
                            throw std::system_error(GetLastError(), std::system_category(),
                                                    "CreatePipe");
                        }
                        err_close_handles.push(c2pread);
                        err_close_handles.push(c2pwrite);
                        dup_handles.push(c2pwrite);
                        break;
                    };
                    case DEVNULL: {
                        c2pwrite = _get_devnull();
                        break;
                    };
                    default: {
                        throw std::invalid_argument(
                            formatN("invalid stdout type: %1", int(stdout_dev.data.builtin)));
                    }
                }
                break;
            }
            case IODev::FD: {
                c2pwrite = (HANDLE) _get_osfhandle(stdout_dev.data.fd);
                break;
            }
            case IODev::CFile: {
                c2pwrite = (HANDLE) _get_osfhandle(_fileno(stdout_dev.data.file));
                break;
            }
            default:
                break;
        }
        c2pwrite = _make_inheritable(c2pwrite);
        err_close_handles.push(c2pwrite);

        // stderr
        switch (stderr_dev.kind) {
            case IODev::Null: {
                errwrite = GetStdHandle(STD_ERROR_HANDLE);
                if (errwrite == INVALID_HANDLE_VALUE) {
                    HANDLE tmp_pipe;
                    if (!CreatePipe(&tmp_pipe, &errwrite, nullptr, 0)) {
                        throw std::system_error(GetLastError(), std::system_category(),
                                                "CreatePipe");
                    }
                    err_close_handles.push(errwrite);
                    dup_handles.push(errwrite);
                    CloseHandle(tmp_pipe);
                }
                break;
            }
            case IODev::Builtin: {
                switch (stderr_dev.data.builtin) {
                    case PIPE: {
                        if (!CreatePipe(&errread, &errwrite, nullptr, 0)) {
                            throw std::system_error(GetLastError(), std::system_category(),
                                                    "CreatePipe");
                        }
                        err_close_handles.push(errread);
                        err_close_handles.push(errwrite);
                        dup_handles.push(errwrite);
                        break;
                    };
                    case DEVNULL: {
                        errwrite = _get_devnull();
                        break;
                    };
                    case STDOUT: {
                        errwrite = c2pwrite;
                        break;
                    };
                }
                break;
            }
            case IODev::FD: {
                errwrite = (HANDLE) _get_osfhandle(stderr_dev.data.fd);
                break;
            }
            case IODev::CFile: {
                errwrite = (HANDLE) _get_osfhandle(_fileno(stderr_dev.data.file));
                break;
            }
            default:
                break;
        }
        errwrite = _make_inheritable(errwrite);
        err_close_handles.push(errwrite);

        // release guard
        err_close_handles.ok = true;

        // close newly-created handles but duplicated
        dup_handles.free();

        return {p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite};
    }


    void Popen::Impl::_close_pipe_fds(Handle p2cread, Handle p2cwrite, Handle c2pread,
                                      Handle c2pwrite, Handle errread, Handle errwrite) {
        _close_pipe_fds_1(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);
        _closed_child_pipe_fds = true;
    }

    void Popen::Impl::_close_pipe_fds_1(Handle p2cread, Handle p2cwrite, Handle c2pread,
                                        Handle c2pwrite, Handle errread, Handle errwrite) {
        if (p2cread != InvalidHandle) {
            CloseHandle(p2cread);
        }
        if (c2pwrite != InvalidHandle) {
            CloseHandle(c2pwrite);
        }
        if (errwrite != InvalidHandle) {
            CloseHandle(errwrite);
        }
        if (_devnull) {
            CloseHandle(_devnull);
            _devnull = nullptr;
        }
    }

    static fs::path _get_command_prompt_path() {
        auto comspec = winGetEnvironmentVariable(L"ComSpec", nullptr);
        fs::path comspec_path;
        if (comspec.empty()) {
            auto system_root = winGetEnvironmentVariable(L"SystemRoot", nullptr);
            if (system_root.empty()) {
                throw std::filesystem::filesystem_error(
                    "shell not found: neither %ComSpec% nor %SystemRoot% is set",
                    std::make_error_code(std::errc::no_such_file_or_directory));
            }
            comspec_path = fs::path(system_root) / L"System32" / L"cmd.exe";
            if (!comspec_path.is_absolute()) {
                throw std::filesystem::filesystem_error(
                    "shell not found: constructed path is not absolute",
                    std::make_error_code(std::errc::no_such_file_or_directory));
            }
        } else {
            comspec_path = comspec;
        }
        return comspec_path;
    }

    // https://github.com/qt/qtbase/blob/6.8.0/src/corelib/io/qprocess_win.cpp#L371
    static std::string _qt_create_commandline(const std::string &program,
                                              const std::vector<std::string> &arguments) {
        std::string args;
        if (!program.empty()) {
            std::string programName = program;
            if (!starts_with(programName, '\"') && ends_with(programName, '\"') &&
                str::contains(programName, ' '))
                programName = '\"' + programName + '\"';
            std::replace(programName.begin(), programName.end(), '/', '\\');

            // add the program as the first arg ... it works better
            args = programName + ' ';
        }

        for (size_t i = 0; i < arguments.size(); ++i) {
            std::string tmp = arguments.at(i);
            // Quotes are escaped and their preceding backslashes are doubled.
            size_t index = tmp.find('"');
            while (index != std::string::npos) {
                // Escape quote
                tmp.insert(tmp.begin() + (index++), '\\');
                // Double preceding backslashes (ignoring the one we just inserted)
                for (size_t i = index - 2; i >= 0 && tmp.at(i) == '\\'; --i) {
                    tmp.insert(tmp.begin() + i, '\\');
                    index++;
                }
                index = tmp.find('"', index + 1);
            }
            if (tmp.empty() || str::contains(tmp, ' ') || str::contains(tmp, '\t')) {
                // The argument must not end with a \ since this would be interpreted
                // as escaping the quote -- rather put the \ behind the quote: e.g.
                // rather use "foo"\ than "foo\"
                size_t i = tmp.size();
                while (i > 0 && tmp.at(i - 1) == '\\')
                    --i;
                tmp.insert(tmp.begin() + i, '"');
                tmp.insert(tmp.begin(), '"');
            }

            // Spaces cannot be placed at the very beginning.
            if (!args.empty())
                args += ' ' + tmp;
            else
                args = tmp;
        }
        return args;
    }

    // https://github.com/python/cpython/blob/3.13/Modules/_winapi.c#L1157
    static inline LPHANDLE _get_handle_list(const llvm::SmallVector<HANDLE, 10> &handles,
                                            SIZE_T *size) {
        if (handles.empty()) {
            return nullptr;
        }

        *size = handles.size() * sizeof(HANDLE);

        LPHANDLE ret = (LPHANDLE) HeapAlloc(GetProcessHeap(), 0, *size);
        for (size_t i = 0; i < handles.size(); i++) {
            ret[i] = handles[i];
        }
        return ret;
    }

    struct AttributeList {
        LPPROC_THREAD_ATTRIBUTE_LIST attribute_list;
        LPHANDLE handle_list;
    };

    // https://github.com/python/cpython/blob/3.13/Modules/_winapi.c#L1210
    static void _free_attribute_list(AttributeList *attribute_list) {
        if (attribute_list->attribute_list != NULL) {
            DeleteProcThreadAttributeList(attribute_list->attribute_list);
            HeapFree(GetProcessHeap(), 0, attribute_list->attribute_list);
        }
        HeapFree(GetProcessHeap(), 0, attribute_list->handle_list);
        memset(attribute_list, 0, sizeof(*attribute_list));
    }

    // https://github.com/python/cpython/blob/3.13/Modules/_winapi.c#L1223
    static std::system_error _get_attribute_list(const llvm::SmallVector<HANDLE, 10> &handles,
                                                 AttributeList *attribute_list) {
        DWORD err;
        const char *where = nullptr;
        BOOL result;

        SIZE_T handle_list_size;
        int attribute_count = 0;
        SIZE_T attribute_list_size = 0;

        // https://github.com/python/cpython/blob/3.13/Modules/_winapi.c#L1251
        attribute_list->handle_list = _get_handle_list(handles, &handle_list_size);
        if (attribute_list->handle_list) {
            attribute_count++;
        }

        /* Get how many bytes we need for the attribute list */
        result = InitializeProcThreadAttributeList(NULL, attribute_count, 0, &attribute_list_size);
        if (result || ((err = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)) {
            where = "InitializeProcThreadAttributeList";
            goto cleanup;
        }

        attribute_list->attribute_list =
            (LPPROC_THREAD_ATTRIBUTE_LIST) HeapAlloc(GetProcessHeap(), 0, attribute_list_size);
        if (!attribute_list->attribute_list) {
            err = GetLastError();
            where = "HeapAlloc";
            goto cleanup;
        }

        result = InitializeProcThreadAttributeList(attribute_list->attribute_list, attribute_count,
                                                   0, &attribute_list_size);
        if (!result) {
            err = GetLastError();

            /* So that we won't call DeleteProcThreadAttributeList */
            HeapFree(GetProcessHeap(), 0, attribute_list->attribute_list);
            attribute_list->attribute_list = NULL;

            where = "InitializeProcThreadAttributeList";
            goto cleanup;
        }

        if (attribute_list->handle_list != NULL) {
            result = UpdateProcThreadAttribute(
                attribute_list->attribute_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                attribute_list->handle_list, handle_list_size, NULL, NULL);
            if (!result) {
                err = GetLastError();
                where = "UpdateProcThreadAttribute";
                goto cleanup;
            }
        }

    cleanup:
        if (where) {
            _free_attribute_list(attribute_list);
            return std::system_error(err, std::system_category(), where);
        }
        return std::system_error(ERROR_SUCCESS, std::system_category());
    }

    // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L1449
    void Popen::Impl::_execute_child(Handle p2cread, Handle p2cwrite, Handle c2pread,
                                     Handle c2pwrite, Handle errread, Handle errwrite) {
        std::string arg_str = _qt_create_commandline({}, args);

        STARTUPINFOEXW siex;
        ZeroMemory(&siex, sizeof(siex));
        STARTUPINFOW &si = siex.StartupInfo;
        si.cb = sizeof(siex);
        if (startupinfo) {
            si.dwFlags = startupinfo->dwFlags;
            si.hStdInput = startupinfo->hStdInput;
            si.hStdOutput = startupinfo->hStdOutput;
            si.hStdError = startupinfo->hStdError;
            si.wShowWindow = startupinfo->wShowWindow;
        }

        bool use_std_handles = false;
        if (p2cread != InvalidHandle && c2pwrite != InvalidHandle && errwrite != InvalidHandle) {
            use_std_handles = true;
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdInput = p2cread;
            si.hStdOutput = c2pwrite;
            si.hStdError = errwrite;
        }

        // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L1495
        llvm::SmallVector<HANDLE, 10> handle_list;
        if (startupinfo) {
            auto it = startupinfo->lpAttributeList.find("handle_list");
            if (it != startupinfo->lpAttributeList.end()) {
                HANDLE *handles = (HANDLE *) it->second;
                for (int i = 0; handles[i] != INVALID_HANDLE_VALUE; i++) {
                    handle_list.push_back(handles[i]);
                }
            }
        }

        if (!handle_list.empty() || (use_std_handles && close_fds)) {
            if (use_std_handles) {
                handle_list.push_back(p2cread);
                handle_list.push_back(c2pwrite);
                handle_list.push_back(errwrite);
            }

            handle_list = _filter_handle_list(handle_list);

            if (!handle_list.empty() && !close_fds) {
                fprintf(stderr, "startupinfo.lpAttributeList['handle_list'] "
                                "overriding close_fds\n");
            }

            // When using the handle_list we always request to inherit
            // handles but the only handles that will be inherited are
            // the ones in the handle_list
            close_fds = false;
        }

        // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L1522
        // prepare shell arguments
        if (shell) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            if (executable.empty()) {
                executable = _get_command_prompt_path();
            }
            arg_str = formatN(R"(%1 /c "%2")", executable, arg_str);
        }

        std::wstring applicationName = executable;
        std::wstring commandLine = wstring_conv::from_utf8(arg_str);

        // https://github.com/python/cpython/blob/3.13/Modules/_winapi.c#L1373
        // prepare environment variables
        std::vector<wchar_t> envStr;
        if (!env.empty()) {
            for (const auto &item : env) {
                envStr.insert(envStr.end(), item.first.begin(), item.first.end());
                envStr.push_back(L'=');
                envStr.insert(envStr.end(), item.second.begin(), item.second.end());
                envStr.push_back(L'\0');
            }
            envStr.push_back(L'\0');
        }

        DWORD dwCreationFlags;
        PROCESS_INFORMATION pi;

        // https://github.com/python/cpython/blob/3.13/Modules/_winapi.c#L1380
        AttributeList attribute_list = {0};
        std::system_error err = _get_attribute_list(handle_list, &attribute_list);
        if (err.code().value() != ERROR_SUCCESS) {
            goto cleanup;
        }

        dwCreationFlags = creationflags | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;

        // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L1551
        if (!CreateProcessW(applicationName.empty() ? NULL : applicationName.data(), //
                            commandLine.data(),                                      //
                            nullptr,                                                 //
                            nullptr,                                                 //
                            !close_fds,                                              //
                            dwCreationFlags,                                         //
                            envStr.empty() ? NULL : envStr.data(),                   //
                            cwd.empty() ? NULL : cwd.c_str(),
                            (LPSTARTUPINFOW) &siex, //
                            &pi                     //
                            )) {
            err = std::system_error(GetLastError(), std::system_category(), "CreateProcessW");
            goto cleanup;
        }

        _handle = pi.hProcess;
        CloseHandle(pi.hThread);
        pid = pi.dwProcessId;

        _child_created = true;

    cleanup:
        _free_attribute_list(&attribute_list);

        // Child is launched. Close the parent's copy of those pipe
        // handles that only the child should have open.  You need
        // to make sure that no handles to the write end of the
        // output pipe are maintained in this process or else the
        // pipe will not close when the child process exits and the
        // ReadFile will hang.
        _close_pipe_fds(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);

        if (err.code().value() != ERROR_SUCCESS) {
            throw err;
        }
    }

    bool Popen::Impl::_internal_poll() {
        error_code.clear();

        if (returncode) {
            return true;
        }
        DWORD result = WaitForSingleObject(_handle, 0);
        switch (result) {
            case WAIT_OBJECT_0: {
                DWORD exitCode;
                if (!GetExitCodeProcess(_handle, &exitCode)) {
                    error_code.assign(GetLastError(), std::system_category());
                    return false;
                }
                returncode = exitCode;
                close_std_files();
                return true;
            }
            case WAIT_FAILED: {
                error_code.assign(GetLastError(), std::system_category());
                return false;
            }
            default:
                break;
        }
        error_code.assign(GetLastError(), std::system_category());
        return false;
    }

    bool Popen::Impl::_wait(int timeout) {
        error_code.clear();

        if (returncode) {
            return true;
        }
        if (timeout < 0) {
            timeout = INFINITE;
        }

        DWORD result = WaitForSingleObject(_handle, timeout);
        switch (result) {
            case WAIT_TIMEOUT: {
                return false;
            }
            case WAIT_FAILED: {
                error_code.assign(GetLastError(), std::system_category());
                return false;
            }
            default:
                break;
        }

        DWORD exitCode;
        if (!GetExitCodeProcess(_handle, &exitCode)) {
            error_code.assign(GetLastError(), std::system_category());
            return false;
        }
        returncode = exitCode;
        close_std_files();
        return true;
    }

    bool Popen::Impl::kill_impl() {
        return terminate_impl();
    }

    bool Popen::Impl::terminate_impl() {
        error_code.clear();

        if (returncode) {
            return true;
        }
        if (TerminateProcess(_handle, KillProcessExitCode)) {
            return true;
        }

        auto err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            DWORD exitCode;

            std::ignore = GetExitCodeProcess(_handle, &exitCode);
            if (exitCode == STILL_ACTIVE) {
                error_code.assign(err, std::system_category());
                return false;
            }
            return true;
        }
        error_code.assign(err, std::system_category());
        return false;
    }

    bool Popen::Impl::send_signal_impl(int sig) {
        // TODO
        return false;
    }

    std::tuple<std::string, std::string> Popen::Impl::communicate_impl(const std::string &input,
                                                                       int timeout) {
        // TODO
        return {};
    }

}