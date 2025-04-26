#include "process.h"

#include <stdexcept>

#include <stdcorelib/platform/osapi_win.h>
#include <stdcorelib/str.h>

namespace stdc::experimental {

    /*!
        \internal
    */
    static int ExecuteProcessImpl(const std::wstring &command,
                                  const std::vector<std::wstring> &args, const std::wstring &cwd,
                                  std::string *output, const std::wstring &stdoutFile = {},
                                  const std::wstring &stderrFile = {}) {
        SECURITY_ATTRIBUTES saAttr = {
            sizeof(SECURITY_ATTRIBUTES),
            nullptr,
            TRUE,
        };
        HANDLE hChildStdoutRd = nullptr, hChildStdoutWr = nullptr;
        HANDLE hStdOutput = nullptr, hStdError = nullptr;
        bool needCapture = false;
        bool showWindow = false;

        // ████████ 标记是否需要关闭句柄 ████████
        bool closeStdOutput = false;
        bool closeStdError = false;

        // ████████ 处理 stdout 重定向 ████████
        if (!stdoutFile.empty()) {
            if (stdoutFile == L"-") {
                // 重定向到父进程终端
                hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
                showWindow = true; // 需要显示窗口以输出到父进程终端
            } else {
                // 重定向到文件
                hStdOutput = CreateFileW(stdoutFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                         &saAttr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hStdOutput == INVALID_HANDLE_VALUE) {
                    throw std::runtime_error("ExecuteProcess failed: failed to open stdout file");
                }
                closeStdOutput = true; // 文件句柄需要关闭
            }
        } else {
            if (output != nullptr) {
                needCapture = true;

                // 创建管道用于捕获输出
                if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
                    throw std::runtime_error("ExecuteProcess failed: failed to create stdout pipe");
                }
                if (!SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0)) {
                    CloseHandle(hChildStdoutRd);
                    CloseHandle(hChildStdoutWr);
                    throw std::runtime_error(
                        "ExecuteProcess failed: failed to set stdout pipe information");
                }
                hStdOutput = hChildStdoutWr;
            } else {
                // 不捕获且无重定向，输出到NUL
                hStdOutput = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         &saAttr, OPEN_EXISTING, 0, nullptr);
                if (hStdOutput == INVALID_HANDLE_VALUE) {
                    throw std::runtime_error("ExecuteProcess failed: failed to open NUL device");
                }
            }
        }

        // ████████ 处理 stderr 重定向 ████████
        if (!stderrFile.empty()) {
            if (stderrFile == L"-") {
                // 重定向到父进程终端
                hStdError = GetStdHandle(STD_ERROR_HANDLE);
                showWindow = true; // 需要显示窗口以输出到父进程终端
            } else {
                // 重定向到文件
                hStdError = CreateFileW(stderrFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &saAttr,
                                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hStdError == INVALID_HANDLE_VALUE) {
                    if (closeStdOutput)
                        CloseHandle(hStdOutput);
                    throw std::runtime_error("ExecuteProcess failed: failed to open stderr file");
                }
                closeStdError = true; // 文件句柄需要关闭
            }
        } else {
            // 未指定stderr重定向，默认输出到NUL
            hStdError = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    &saAttr, OPEN_EXISTING, 0, nullptr);
            if (hStdError == INVALID_HANDLE_VALUE) {
                if (closeStdOutput)
                    CloseHandle(hStdOutput);
                throw std::runtime_error("ExecuteProcess failed: failed to open NUL device");
            }
        }

        // ████████ 拼接命令行 ████████
        std::wstring cmdLine = command;
        for (size_t i = 0; i < args.size(); ++i) {
            cmdLine += L' ';
            if (args[i].find(L' ') != std::wstring::npos) {
                cmdLine += L'"' + args[i] + L'"';
            } else {
                cmdLine += args[i];
            }
        }

        // ████████ 配置子进程启动信息 ████████
        STARTUPINFOW si = {sizeof(si)};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = hStdOutput;
        si.hStdError = hStdError;

        // ████████ 动态设置窗口标志 ████████
        DWORD dwCreationFlags = showWindow ? 0 : CREATE_NO_WINDOW;

        // ████████ 创建进程 ████████
        PROCESS_INFORMATION pi = {};
        if (!CreateProcessW(NULL, cmdLine.data(), nullptr, nullptr,
                            TRUE, // 继承句柄
                            dwCreationFlags, nullptr, cwd.empty() ? NULL : cwd.c_str(), &si, &pi)) {
            DWORD err = GetLastError();
            if (closeStdOutput)
                CloseHandle(hStdOutput);
            if (closeStdError)
                CloseHandle(hStdError);
            if (needCapture) {
                CloseHandle(hChildStdoutRd);
                CloseHandle(hChildStdoutWr);
            }
            throw std::runtime_error("ExecuteProcess failed: failed to create process: " +
                                     stdc::wstring_conv::to_utf8(stdc::winFormatError(err, 0)));
        }

        // ████████ 安全关闭句柄 ████████
        if (closeStdOutput)
            CloseHandle(hStdOutput);
        if (closeStdError)
            CloseHandle(hStdError);

        // ████████ 捕获输出到output ████████
        if (needCapture) {
            CloseHandle(hChildStdoutWr);

            char buffer[4096];
            DWORD bytesRead;
            std::string childOutput;
            while (ReadFile(hChildStdoutRd, buffer, sizeof(buffer), &bytesRead, nullptr) &&
                   bytesRead > 0) {
                childOutput.append(buffer, bytesRead);
            }
            *output = childOutput;
            CloseHandle(hChildStdoutRd);
        }

        // ████████ 等待进程退出 ████████
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode;
    }

    static std::vector<std::wstring> ToWideArgs(const std::vector<std::string> &args) {
        std::vector<std::wstring> wide_args;
        wide_args.reserve(args.size());
        for (const auto &arg : args) {
            wide_args.push_back(stdc::wstring_conv::from_utf8(arg));
        }
        return wide_args;
    }

    int Process::start(const std::filesystem::path &command, const std::vector<std::string> &args,
                       const std::filesystem::path &cwd, const std::string &stdoutFile,
                       const std::string &stderrFile) {
        int ret = ExecuteProcessImpl(command, ToWideArgs(args), cwd, nullptr,
                                     stdc::wstring_conv::from_utf8(stdoutFile),
                                     stdc::wstring_conv::from_utf8(stderrFile));
        return ret;
    }

    int Process::checkOptput(const std::filesystem::path &command,
                             const std::vector<std::string> &args, const std::filesystem::path &cwd,
                             std::string &output) {
        return ExecuteProcessImpl(command, ToWideArgs(args), cwd, &output);
    }

}