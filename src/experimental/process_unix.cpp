#include "process.h"

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <stdexcept>
#include <cstring>
#include <iostream>

namespace stdc::experimental {

    /*!
        \internal
    */
    static int ExecuteProcessImpl(const std::string &command, const std::vector<std::string> &args,
                                  const std::string &cwd,
                                  const std::map<std::string, std::string> &env,
                                  std::string *output, const std::string &stdoutFile = {},
                                  const std::string &stderrFile = {}) {
        // 标准输出处理相关描述符
        int stdoutFd = -1;
        int stdoutPipe[2] = {-1, -1};
        bool captureOutput = (output != nullptr) && stdoutFile.empty();

        // 处理标准输出重定向
        if (!stdoutFile.empty()) {
            if (stdoutFile == "-") {
                // 继承父进程标准输出，无需操作
            } else {
                stdoutFd = open(stdoutFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (stdoutFd == -1) {
                    throw std::runtime_error("ExecuteProcess failed: failed to open stdout file");
                }
            }
        } else {
            if (captureOutput) {
                if (pipe(stdoutPipe) == -1) {
                    throw std::runtime_error("ExecuteProcess failed: failed to create stdout pipe");
                }
            } else {
                stdoutFd = open("/dev/null", O_WRONLY);
                if (stdoutFd == -1) {
                    throw std::runtime_error("ExecuteProcess failed: failed to open /dev/null");
                }
            }
        }

        // 处理标准错误重定向
        int stderrFd = -1;
        if (!stderrFile.empty()) {
            if (stderrFile == "-") {
                // 继承父进程标准错误
            } else {
                stderrFd = open(stderrFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (stderrFd == -1) {
                    if (stdoutFd != -1)
                        close(stdoutFd);
                    if (captureOutput) {
                        close(stdoutPipe[0]);
                        close(stdoutPipe[1]);
                    }
                    throw std::runtime_error("ExecuteProcess failed: failed to open stderr file");
                }
            }
        } else {
            stderrFd = open("/dev/null", O_WRONLY);
            if (stderrFd == -1) {
                if (stdoutFd != -1)
                    close(stdoutFd);
                if (captureOutput) {
                    close(stdoutPipe[0]);
                    close(stdoutPipe[1]);
                }
                throw std::runtime_error("ExecuteProcess failed: failed to open /dev/null");
            }
        }

        // 创建子进程
        pid_t pid = fork();
        if (pid == -1) {
            if (stdoutFd != -1)
                close(stdoutFd);
            if (stderrFd != -1)
                close(stderrFd);
            if (captureOutput) {
                close(stdoutPipe[0]);
                close(stdoutPipe[1]);
            }
            throw std::runtime_error("ExecuteProcess failed: failed to fork process");
        }

        if (pid == 0) { // 子进程
            // 新增工作目录设置
            if (!cwd.empty()) {
                if (chdir(cwd.c_str()) != 0) {
                    const char *err = "Failed to change working directory\n";
                    write(STDERR_FILENO, err, strlen(err));
                    exit(126); // 使用特殊退出码表示目录设置失败
                }
            }

            // 处理标准输出重定向
            if (stdoutFd != -1) {
                dup2(stdoutFd, STDOUT_FILENO);
                close(stdoutFd);
            } else if (captureOutput) {
                close(stdoutPipe[0]); // 关闭读端
                dup2(stdoutPipe[1], STDOUT_FILENO);
                close(stdoutPipe[1]);
            }

            // 处理标准错误重定向
            if (stderrFd != -1) {
                dup2(stderrFd, STDERR_FILENO);
                close(stderrFd);
            }

            // 刷新所有 C 标准流缓冲区
            fflush(stdout);
            fflush(stderr);

            // 刷新 C++ 标准流缓冲区
            std::cout.flush();
            std::cerr.flush();

            // 构建参数数组
            std::vector<char *> argv;
            argv.reserve(args.size() + 2);
            argv.push_back(const_cast<char *>(command.c_str()));
            for (const auto &arg : args) {
                argv.push_back(const_cast<char *>(arg.c_str()));
            }
            argv.push_back(nullptr);

            // 执行程序
            if (env.empty()) {
                execvp(command.c_str(), argv.data());
            } else {
                std::vector<std::string> envbuf;
                envbuf.reserve(env.size());
                for (const auto &item : env) {
                    envbuf.push_back(item.first + "=" + item.second);
                }

                std::vector<char *> envp;
                envp.reserve(envbuf.size() + 1);
                for (const auto &entry : envbuf) {
                    envp.push_back(const_cast<char *>(entry.c_str()));
                }
                envp.push_back(nullptr);
                execvpe(command.c_str(), argv.data(), envp.data());
            }
            exit(127); // exec失败时的退出码
        }

        // 父进程
        if (stdoutFd != -1)
            close(stdoutFd);
        if (stderrFd != -1)
            close(stderrFd);

        // 捕获输出处理
        if (captureOutput) {
            close(stdoutPipe[1]); // 关闭写端
            char buffer[4096];
            ssize_t bytesRead;
            std::string childOutput;

            while ((bytesRead = read(stdoutPipe[0], buffer, sizeof(buffer))) > 0) {
                childOutput.append(buffer, bytesRead);
            }
            *output = childOutput;
            close(stdoutPipe[0]);
        }

        // 等待子进程结束
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -1; // 非正常退出
    }

    int Process::start(const std::filesystem::path &command, const std::vector<std::string> &args,
                       const std::filesystem::path &cwd,
                       const std::map<std::string, std::string> &env, const std::string &stdoutFile,
                       const std::string &stderrFile) {
        int ret = ExecuteProcessImpl(command, args, cwd, env, nullptr, stdoutFile, stderrFile);
        return ret;
    }

    int Process::checkOptput(const std::filesystem::path &command,
                             const std::vector<std::string> &args, const std::filesystem::path &cwd,
                             const std::map<std::string, std::string> &env, std::string &output) {
        return ExecuteProcessImpl(command, args, cwd, env, &output);
    }

#ifndef __APPLE__
    std::map<std::string, std::string> Process::systemEnvironment() {
        std::map<std::string, std::string> env;
        const char *entry;
        for (int count = 0; (entry = environ[count]); ++count) {
            const char *equal = strchr(entry, '=');
            if (!equal)
                continue;

            std::string name(entry, equal - entry);
            std::string value(equal + 1);
            env.insert(std::make_pair(name, value));
        }
        return env;
    }
#endif

}