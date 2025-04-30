#include "popen.h"
#include "popen_p.h"

#include <fcntl.h>
#include <io.h>

#include <array>
#include <algorithm>
#include <cassert>

#include "winapi.h"
#include "str.h"
#include "scope_guard.h"
#include "3rdparty/llvm/smallvector.h"

#include "3rdparty/llvm/smallvector.h"

namespace fs = std::filesystem;

namespace stdc {

    using namespace winapi;

    constexpr UINT KillProcessExitCode = 0xf291;

    void Popen::Impl::_cleanup() {
        close_std_files();

        CloseHandle(_handle);
        _handle = InvalidHandle;

        pid = -1;
        tid = -1;
    }

    static inline std::error_code make_last_error_code() {
        return std::error_code(GetLastError(), std::system_category());
    }

    bool Popen::Impl::_get_devnull() {
        // Create a null device handle
        auto handle =
            CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            error_code = make_last_error_code();
            error_api = "CreateFileW";
            return false;
        }
        _devnull = handle;
        return true;
    }

    // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L1436
    //
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
    bool Popen::Impl::_get_handles(HANDLE &p2cread, HANDLE &p2cwrite, HANDLE &c2pread,
                                   HANDLE &c2pwrite, HANDLE &errread, HANDLE &errwrite) {
        if (stdin_dev.kind == 0 && stdout_dev.kind == 0 && stderr_dev.kind == 0) {
            return true;
        }

        //
        // helpers and guards
        //

        // handles that should be closed if error occurs
        std::array<HANDLE, 10> err_close_handles;
        int err_close_handles_cnt = 0;
        auto err_close_handle_guard = make_scope_guard([&]() {
            for (int i = 0; i < err_close_handles_cnt; i++) {
                CloseHandle(err_close_handles[i]);
            }
            if (_devnull != InvalidHandle) {
                CloseHandle(_devnull);
                _devnull = InvalidHandle;
            }
        });
        const auto &push_err_close_handle = [&](HANDLE handle) {
            err_close_handles[err_close_handles_cnt++] = handle;
        };

        // handles that should be closed anyway
        std::array<HANDLE, 10> dup_close_handles;
        int dup_close_handles_cnt = 0;
        auto dup_close_handle_guard = make_scope_guard([&]() {
            for (int i = 0; i < dup_close_handles_cnt; i++) {
                CloseHandle(dup_close_handles[i]);
            }
        });
        const auto &push_dup_close_handle = [&](HANDLE handle) {
            dup_close_handles[dup_close_handles_cnt++] = handle;
        };

        // duplicate the handle that needs to be inherited
        const auto &make_inheritable = [this](HANDLE &handle) {
            HANDLE target_handle;
            if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &target_handle,
                                 0, 1, DUPLICATE_SAME_ACCESS)) {
                error_code = make_last_error_code();
                error_api = "DuplicateHandle";
                return false;
            }
            handle = target_handle;
            return true;
        };

        // convert a file descriptor to a handle
        const auto &convert_from_fd = [this](HANDLE &handle, int fd) {
            auto tmp_handle = (HANDLE) _get_osfhandle(fd);
            if (tmp_handle == INVALID_HANDLE_VALUE) {
                error_code = std::error_code(ERROR_INVALID_HANDLE, std::system_category());
                error_api = "_get_osfhandle";
                return false;
            };
            handle = tmp_handle;
            return true;
        };

        // create a pipe
        const auto &create_pipe = [this](HANDLE &read_handle, HANDLE &write_handle) {
            if (!CreatePipe(&read_handle, &write_handle, nullptr, 0)) {
                error_code = make_last_error_code();
                error_api = "CreatePipe";
                return false;
            }
            return true;
        };

        // open or return devnull
        const auto &open_devnull = [this](HANDLE &handle) {
            if (_devnull == InvalidHandle && !_get_devnull()) {
                return false;
            }
            handle = _devnull;
            return true;
        };

        //
        // transaction start
        //

        // stdin
        switch (stdin_dev.kind) {
            case IODev::None: {
                p2cread = GetStdHandle(STD_INPUT_HANDLE);
                if (p2cread == INVALID_HANDLE_VALUE) {
                    HANDLE tmp_pipe;
                    if (!create_pipe(p2cread, tmp_pipe)) {
                        return false;
                    }
                    push_dup_close_handle(p2cread);
                    CloseHandle(tmp_pipe);
                }
                break;
            }
            case IODev::Builtin: {
                switch (stdin_dev.data.builtin) {
                    case PIPE: {
                        if (!create_pipe(p2cread, p2cwrite)) {
                            return false;
                        }
                        push_dup_close_handle(p2cread);
                        push_err_close_handle(p2cwrite);
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
                if (!convert_from_fd(p2cread, _fileno(stdin_dev.data.file))) {
                    return false;
                }
                break;
            }
            default:
                break;
        }
        if (!make_inheritable(p2cread)) {
            return false;
        }
        push_err_close_handle(p2cread);

        // stdout
        switch (stdout_dev.kind) {
            case IODev::None: {
                c2pwrite = GetStdHandle(STD_OUTPUT_HANDLE);
                if (c2pwrite == INVALID_HANDLE_VALUE) {
                    HANDLE tmp_pipe;
                    if (!create_pipe(tmp_pipe, c2pwrite)) {
                        return false;
                    }
                    push_dup_close_handle(c2pwrite);
                    CloseHandle(tmp_pipe);
                }
                break;
            }
            case IODev::Builtin: {
                switch (stdout_dev.data.builtin) {
                    case PIPE: {
                        if (!create_pipe(c2pread, c2pwrite)) {
                            return false;
                        }
                        push_err_close_handle(c2pread);
                        push_dup_close_handle(c2pwrite);
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
                if (!convert_from_fd(c2pwrite, _fileno(stdout_dev.data.file))) {
                    return false;
                }
                break;
            }
            default:
                break;
        }
        if (!make_inheritable(c2pwrite)) {
            return false;
        }
        push_err_close_handle(c2pwrite);

        // stderr
        switch (stderr_dev.kind) {
            case IODev::None: {
                errwrite = GetStdHandle(STD_ERROR_HANDLE);
                if (errwrite == INVALID_HANDLE_VALUE) {
                    HANDLE tmp_pipe;
                    if (!create_pipe(tmp_pipe, errwrite)) {
                        return false;
                    }
                    push_dup_close_handle(errwrite);
                    CloseHandle(tmp_pipe);
                }
                break;
            }
            case IODev::Builtin: {
                switch (stderr_dev.data.builtin) {
                    case PIPE: {
                        if (!create_pipe(errread, errwrite)) {
                            return false;
                        }
                        push_err_close_handle(errread);
                        push_dup_close_handle(errwrite);
                        break;
                    };
                    case DEVNULL: {
                        if (!open_devnull(errwrite)) {
                            return false;
                        }
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
                if (!convert_from_fd(errwrite, stderr_dev.data.fd)) {
                    return false;
                }
                break;
            }
            case IODev::CFile: {
                if (!convert_from_fd(errwrite, _fileno(stderr_dev.data.file))) {
                    return false;
                }
                break;
            }
            default:
                break;
        }
        if (!make_inheritable(errwrite)) {
            return false;
        }

        //
        // transaction end
        //

        // release guard
        err_close_handle_guard.dismiss();
        return true;
    }

    void Popen::Impl::_close_pipe_fds(Handle p2cread, int p2cwrite, int c2pread, Handle c2pwrite,
                                      int errread, Handle errwrite) {
        _close_pipe_fds_1(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);
        _closed_child_pipe_fds = true;
    }

    void Popen::Impl::_close_pipe_fds_1(Handle p2cread, int p2cwrite, int c2pread, Handle c2pwrite,
                                        int errread, Handle errwrite) {
        (void) p2cwrite;
        (void) c2pread;
        (void) errread;

        if (p2cread != InvalidHandle) {
            CloseHandle(p2cread);
        }
        if (c2pwrite != InvalidHandle) {
            CloseHandle(c2pwrite);
        }
        if (errwrite != InvalidHandle) {
            CloseHandle(errwrite);
        }
        if (_devnull != InvalidHandle) {
            CloseHandle(_devnull);
            _devnull = InvalidHandle;
        }
    }

    static fs::path _get_command_prompt_path(std::string &err_msg) {
        auto comspec = kernel32::GetEnvironmentVariableW(L"ComSpec", nullptr);
        fs::path comspec_path;
        if (comspec.empty()) {
            auto system_root = kernel32::GetEnvironmentVariableW(L"SystemRoot", nullptr);
            if (system_root.empty()) {
                err_msg = "shell not found: neither %ComSpec% nor %SystemRoot% is set";
                return {};
            }
            comspec_path = fs::path(system_root) / L"System32" / L"cmd.exe";
            if (!comspec_path.is_absolute()) {
                err_msg = "shell not found: constructed path is not absolute";
                return {};
            }
        } else {
            comspec_path = comspec;
        }
        return comspec_path;
    }

    // https://github.com/qt/qtbase/blob/6.8.0/src/corelib/io/qprocess_win.cpp#L371
    static std::string qt_create_commandline(const std::string &program,
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
    static std::tuple<std::error_code, const char *>
        _get_attribute_list(const llvm::SmallVector<HANDLE, 10> &handles,
                            AttributeList *attribute_list) {
        DWORD err;
        const char *err_api = nullptr;
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
            err_api = "InitializeProcThreadAttributeList";
            goto _cleanup;
        }

        attribute_list->attribute_list =
            (LPPROC_THREAD_ATTRIBUTE_LIST) HeapAlloc(GetProcessHeap(), 0, attribute_list_size);
        if (!attribute_list->attribute_list) {
            err = GetLastError();
            err_api = "HeapAlloc";
            goto _cleanup;
        }

        result = InitializeProcThreadAttributeList(attribute_list->attribute_list, attribute_count,
                                                   0, &attribute_list_size);
        if (!result) {
            err = GetLastError();

            /* So that we won't call DeleteProcThreadAttributeList */
            HeapFree(GetProcessHeap(), 0, attribute_list->attribute_list);
            attribute_list->attribute_list = NULL;

            err_api = "InitializeProcThreadAttributeList";
            goto _cleanup;
        }

        if (attribute_list->handle_list != NULL) {
            result = UpdateProcThreadAttribute(
                attribute_list->attribute_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                attribute_list->handle_list, handle_list_size, NULL, NULL);
            if (!result) {
                err = GetLastError();
                err_api = "UpdateProcThreadAttribute";
                goto _cleanup;
            }
        }

    _cleanup:
        if (err_api) {
            _free_attribute_list(attribute_list);
            return {std::error_code(err, std::system_category()), err_api};
        }
        return {{}, {}};
    }

    // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L1449
    bool Popen::Impl::_execute_child(Handle p2cread, int p2cwrite, int c2pread, Handle c2pwrite,
                                     int errread, Handle errwrite) {
        assert(!args.empty());

        std::string args_str = qt_create_commandline({}, args);

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
                fprintf(stderr, "stdc::Popen: %s\n", //
                        "startupinfo.lpAttributeList['handle_list'] overriding close_fds");
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
                std::string err_msg;
                executable = _get_command_prompt_path(err_msg);
                if (!err_msg.empty()) {
                    error_code = std::make_error_code(std::errc::no_such_file_or_directory);
                    error_msg = err_msg;
                    return false;
                }
            }
            args_str = formatN(R"(%1 /c "%2")", executable, args_str);
        }

        std::wstring application_name = executable;
        std::wstring command_line = wstring_conv::from_utf8(args_str);

        // https://github.com/python/cpython/blob/3.13/Modules/_winapi.c#L1373
        // prepare environment variables
        llvm::SmallVector<wchar_t, 1024> env_str;
        if (!env.empty()) {
            for (const auto &item : env) {
                env_str.insert(env_str.end(), item.first.begin(), item.first.end());
                env_str.push_back(L'=');
                env_str.insert(env_str.end(), item.second.begin(), item.second.end());
                env_str.push_back(L'\0');
            }
            env_str.push_back(L'\0');
        }

        DWORD dwCreationFlags;
        PROCESS_INFORMATION pi;

        // https://github.com/python/cpython/blob/3.13/Modules/_winapi.c#L1380
        AttributeList attribute_list = {0};
        if (auto [ec, err_api] = _get_attribute_list(handle_list, &attribute_list);
            ec.value() != 0) {
            error_code = ec;
            error_api = err_api;
            goto _cleanup;
        }

        dwCreationFlags = creationflags | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;

        // https://github.com/python/cpython/blob/3.13/Lib/subprocess.py#L1551
        if (!CreateProcessW(application_name.empty() ? NULL : application_name.data(), //
                            command_line.data(),                                       //
                            nullptr,                                                   //
                            nullptr,                                                   //
                            !close_fds,                                                //
                            dwCreationFlags,                                           //
                            env_str.empty() ? NULL : env_str.data(),                   //
                            cwd.empty() ? NULL : cwd.c_str(),
                            (LPSTARTUPINFOW) &siex, //
                            &pi                     //
                            )) {
            error_code = make_last_error_code();
            error_api = "CreateProcessW";
            goto _cleanup;
        }

        _handle = pi.hProcess;
        CloseHandle(pi.hThread);
        pid = pi.dwProcessId;
        tid = pi.dwThreadId;

        _child_created = true;

    _cleanup:
        _free_attribute_list(&attribute_list);

        // Child is launched. Close the parent's copy of those pipe
        // handles that only the child should have open.  You need
        // to make sure that no handles to the write end of the
        // output pipe are maintained in this process or else the
        // pipe will not close when the child process exits and the
        // ReadFile will hang.
        _close_pipe_fds(p2cread, p2cwrite, c2pread, c2pwrite, errread, errwrite);

        return error_code.value() == 0;
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
                    error_code = make_last_error_code();
                    return false;
                }
                returncode = exitCode;
                _cleanup();
                return true;
            }
            case WAIT_FAILED: {
                error_code = make_last_error_code();
                return false;
            }
            default:
                break;
        }
        error_code = make_last_error_code();
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
                error_code = make_last_error_code();
                return false;
            }
            default:
                break;
        }

        DWORD exitCode;
        if (!GetExitCodeProcess(_handle, &exitCode)) {
            error_code = make_last_error_code();
            return false;
        }
        returncode = exitCode;
        _cleanup();
        return true;
    }

    bool Popen::Impl::kill_impl() {
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
            returncode = exitCode;
            _cleanup();
            return true;
        }
        error_code.assign(err, std::system_category());
        return false;
    }

    static BOOL CALLBACK qt_terminateApp(HWND hwnd, LPARAM procId) {
        DWORD currentProcId = 0;
        GetWindowThreadProcessId(hwnd, &currentProcId);
        if (currentProcId == (DWORD) procId)
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return TRUE;
    }

    // https://github.com/qt/qtbase/blob/6.8.0/src/corelib/io/qprocess_win.cpp#L641
    bool Popen::Impl::terminate_impl() {
        error_code.clear();

        if (returncode) {
            return true;
        }
        if (!EnumWindows(qt_terminateApp, (LPARAM) pid)) {
            error_code = make_last_error_code();
            return false;
        }
        if (!PostThreadMessageW(tid, WM_CLOSE, 0, 0)) {
            error_code = make_last_error_code();
            return false;
        }
        return true;
    }

    bool Popen::Impl::send_signal_impl(int sig) {
        error_code.clear();

        if (returncode) {
            return true;
        }

        if (sig == CTRL_C_EVENT || sig == CTRL_BREAK_EVENT) {
            if (GenerateConsoleCtrlEvent(sig, pid)) {
                return true;
            }
            error_code = make_last_error_code();
            return false;
        }
        error_code = std::error_code(ERROR_NOT_SUPPORTED, std::system_category());
        return false;
    }

    std::tuple<std::string, std::string> Popen::Impl::communicate_impl(const std::string &input,
                                                                       int timeout) {
        // TODO
        return {};
    }

}