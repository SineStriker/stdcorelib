// SPDX-License-Identifier: MIT

#include <cstdio>
#include <chrono>
#include <iterator>
#include <string>
#include <vector>

#include <stdcorelib/support/popen.h>
#include <stdcorelib/system.h>

#include <boost/test/unit_test.hpp>

#ifdef _WIN32
#  include <stdcorelib/platform/windows/stdc_windows.h>
#else
#  include <csignal>
#  include <cstring>
#  include <dirent.h>
#  include <fcntl.h>
#  include <pwd.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_popen)

namespace {

    // Every wait here is bounded, so a regression hangs the one case rather than the whole run.
    constexpr int Timeout = 15000;

    // The same child behavior spelled in each platform's shell.
#ifdef _WIN32
    const char *ShellExe = "cmd";
    const char *ShellFlag = "/c";
    const char *EchoHello = "echo hello";
    const char *EchoOutErr = "echo out& echo err 1>&2";
    const char *EchoErr = "echo err 1>&2";
    const char *EchoThree = "echo one& echo two& echo three";
    const char *PrintCwd = "cd";
    const char *ExitZero = "exit 0";
    const char *ExitThree = "exit 3";
    const char *BigOutput = "for /L %i in (1,1,5000) do @echo 0123456789012345678901234567890123";
    // Long enough that the process is certainly still up when we look, short enough that a
    // leaked one goes away on its own.
    const char *SleepLong = "ping -n 20 127.0.0.1 >nul";
    const std::vector<std::string> SleepArgs = {"ping", "-n", "20", "127.0.0.1"};
    const std::vector<std::string> FilterX = {"findstr", "x"};
#else
    const char *ShellExe = "/bin/sh";
    const char *ShellFlag = "-c";
    const char *EchoHello = "echo hello";
    const char *EchoOutErr = "echo out; echo err 1>&2";
    const char *EchoErr = "echo err 1>&2";
    const char *EchoThree = "echo one; echo two; echo three";
    const char *PrintCwd = "pwd";
    const char *ExitZero = "exit 0";
    const char *ExitThree = "exit 3";
    const char *BigOutput = "i=0; while [ $i -lt 5000 ]; do "
                            "echo 0123456789012345678901234567890123; i=$((i+1)); done";
    const char *SleepLong = "sleep 20";
    const std::vector<std::string> SleepArgs = {"sleep", "20"};
    const std::vector<std::string> FilterX = {"grep", "x"};
#endif

    // Whether a pid still names a live process, asked from outside the Popen that started it.
    bool process_alive(int pid) {
#ifdef _WIN32
        HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
        if (!h) {
            return false;
        }
        DWORD code = 0;
        bool alive = ::GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
        ::CloseHandle(h);
        return alive;
#else
        return ::kill(pid_t(pid), 0) == 0;
#endif
    }

    void hard_kill(int pid) {
#ifdef _WIN32
        if (HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(pid))) {
            ::TerminateProcess(h, 1);
            ::CloseHandle(h);
        }
#else
        ::kill(pid_t(pid), SIGKILL);
        int status;
        ::waitpid(pid_t(pid), &status, 0);
#endif
    }

    std::vector<std::string> shell_args(const char *script) {
        return {ShellExe, ShellFlag, script};
    }

    // The first line of the child's output. Trailing blanks go too: `echo err 1>&2` in cmd
    // emits the space before the redirection as part of the text.
    std::string first_line(const std::string &s) {
        auto end = s.find_first_of("\r\n");
        std::string line = end == std::string::npos ? s : s.substr(0, end);
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        return line;
    }

}

BOOST_AUTO_TEST_CASE(test_run_and_returncode) {
    {
        Popen p;
        std::string err;
        p.args(shell_args(ExitZero));
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_CHECK(p.pid() > 0);
        BOOST_REQUIRE(p.wait(Timeout));
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }

    // a non-zero exit status comes back as-is
    {
        Popen p;
        std::string err;
        p.args(shell_args(ExitThree));
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_REQUIRE(p.wait(Timeout));
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), 3);
    }

    // starting something that does not exist fails, with a message
    {
        Popen p;
        std::string err;
        p.args({"no_such_program_9f3a"});
        BOOST_CHECK(!p.start(&err));
        BOOST_CHECK(!err.empty());
    }
}

// A failed start is retryable after correcting the setup; stale error state must not turn a
// successfully created child into a reported failure.
BOOST_AUTO_TEST_CASE(test_start_retry) {
    {
        Popen p;
        std::string err;
        p.args(shell_args(ExitZero)).stdin_(Popen::STDOUT);
        BOOST_CHECK(!p.start(&err));
        BOOST_CHECK(p.error_code());

        err.clear();
        p.stdin_(Popen::DEVNULL);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_REQUIRE(p.wait(Timeout));
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }

    // Also cover a failure from CreateProcess/exec, after the pipes and (on POSIX) a short-lived
    // child have already been created.
    {
        Popen p;
        std::string err;
        p.args({"no_such_executable_9f3a"}).stdout_(Popen::PIPE);
        BOOST_CHECK(!p.start(&err));
        BOOST_CHECK(!err.empty());

        err.clear();
        p.args(shell_args(ExitZero));
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        std::ignore = p.communicate({}, Timeout);
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }
}

// A bare name is looked up along PATH, the same way a shell would find it.
BOOST_AUTO_TEST_CASE(test_path_lookup) {
    Popen p;
    std::string err;
#ifdef _WIN32
    p.args({"cmd", "/c", "exit 0"});
#else
    p.args({"echo", "found"}).stdout_(Popen::PIPE);
#endif
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    BOOST_REQUIRE(p.wait(Timeout));
    BOOST_CHECK_EQUAL(*p.returncode(), 0);
}

// wait() used to close the pipes along with the process handle, which threw away the output
// before anyone could read it.
BOOST_AUTO_TEST_CASE(test_output_survives_wait) {
    Popen p;
    std::string err;
    p.args(shell_args(EchoHello)).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    BOOST_REQUIRE(p.wait(Timeout));

    auto &out = p.stdout_();
    BOOST_REQUIRE(out.is_open());
    std::string line;
    BOOST_REQUIRE(std::getline(out, line));
    BOOST_CHECK_EQUAL(first_line(line), "hello");
}

BOOST_AUTO_TEST_CASE(test_communicate) {
    // stdout only
    {
        Popen p;
        std::string err;
        p.args(shell_args(EchoHello)).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        BOOST_CHECK_EQUAL(first_line(out), "hello");
        BOOST_CHECK(errout.empty());
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }

    // stdout and stderr kept apart
    {
        Popen p;
        std::string err;
        p.args(shell_args(EchoOutErr)).stdout_(Popen::PIPE).stderr_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        BOOST_CHECK_EQUAL(first_line(out), "out");
        BOOST_CHECK_EQUAL(first_line(errout), "err");
    }

    // stderr folded into stdout
    {
        Popen p;
        std::string err;
        p.args(shell_args(EchoErr)).stdout_(Popen::PIPE).stderr_(Popen::STDOUT);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        BOOST_CHECK_EQUAL(first_line(out), "err");
        BOOST_CHECK(errout.empty());
    }

    // input is written and the pipe closed, so a filter can finish
    {
        Popen p;
        std::string err;
        p.args(FilterX).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate("abc\nxyz\ndef\n", Timeout);
        BOOST_CHECK_EQUAL(first_line(out), "xyz");
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }

    // more output than a pipe buffer holds, which is the case a single-threaded reader deadlocks
    {
        Popen p;
        std::string err;
        p.args(shell_args(BigOutput)).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        BOOST_CHECK_GT(out.size(), 100000u);
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }

    // a child that outlives the timeout is killed rather than left behind
    {
        Popen p;
        std::string err;
        p.args(FilterX).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        // no input, so the filter would wait forever
        auto [out, errout] = p.communicate("no match here\n", 500);
        BOOST_REQUIRE(p.returncode());
    }

    // The timeout covers writing too. This child never reads stdin, so a synchronous writer
    // would fill the pipe and hang before reaching wait().
    {
        Popen p;
        std::string err;
        p.args(SleepArgs).stdin_(Popen::PIPE).stdout_(Popen::DEVNULL);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        std::string input(1 << 20, 'x');
        auto started = std::chrono::steady_clock::now();
        std::ignore = p.communicate(input, 300);
        auto elapsed = std::chrono::steady_clock::now() - started;
        BOOST_CHECK(elapsed < std::chrono::seconds(5));
        BOOST_CHECK(p.error_code() == std::make_error_code(std::errc::timed_out));
        BOOST_REQUIRE(p.returncode());
    }
}

// Writing to a pipe whose reader has already exited raises SIGPIPE on POSIX, and its default
// action would take the whole test runner with it.
BOOST_AUTO_TEST_CASE(test_write_to_dead_child) {
    Popen p;
    std::string err;
    p.args(shell_args(ExitZero)).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    BOOST_REQUIRE(p.wait(Timeout));

    std::string big(1 << 20, 'x');
    auto [out, errout] = p.communicate(big, Timeout);
    BOOST_REQUIRE(p.returncode());
    BOOST_CHECK_EQUAL(*p.returncode(), 0);
}

// Without a way to close this side of the pipe, a child reading to end of input never returns.
BOOST_AUTO_TEST_CASE(test_close_stdin_ends_input) {
    Popen p;
    std::string err;
    p.args(FilterX).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);

    auto &in = p.stdin_();
    BOOST_REQUIRE(in.is_open());
    in << "xyz\n" << std::flush;

    // still running: nothing has told it the input is over
    BOOST_CHECK(!p.wait(200));
    BOOST_CHECK(!p.returncode());

    p.stdin_().close();
    BOOST_CHECK(!p.stdin_().is_open());

    BOOST_REQUIRE(p.wait(Timeout));
    BOOST_REQUIRE(p.returncode());
    BOOST_CHECK_EQUAL(*p.returncode(), 0);
}

// poll() returning false means "not finished", which is not an error, and it must not pick up
// whatever the last failed system call happened to leave behind.
BOOST_AUTO_TEST_CASE(test_poll) {
    Popen p;
    std::string err;
    p.args(FilterX).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);

    BOOST_CHECK(!p.poll());
    BOOST_CHECK(!p.returncode());
    BOOST_CHECK_EQUAL(p.error_code().value(), 0);

    p.stdin_().close();
    BOOST_REQUIRE(p.wait(Timeout));

    // once it has exited, poll() says so and keeps saying so
    BOOST_CHECK(p.poll());
    BOOST_REQUIRE(p.returncode());
    BOOST_CHECK(p.poll());
    BOOST_CHECK_EQUAL(p.error_code().value(), 0);
}

BOOST_AUTO_TEST_CASE(test_cwd) {
    Popen p;
    std::string err;
#ifdef _WIN32
    const char *dir = "C:\\Windows";
#else
    const char *dir = "/usr";
#endif
    p.args(shell_args(PrintCwd)).cwd(dir).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    auto [out, errout] = p.communicate({}, Timeout);
    BOOST_CHECK_EQUAL(first_line(out), dir);
}

// A working directory that does not exist fails at start, and says which one.
BOOST_AUTO_TEST_CASE(test_bad_cwd) {
    Popen p;
    std::string err;
    p.args(shell_args(ExitZero)).cwd("no_such_dir_9f3a");
    BOOST_CHECK(!p.start(&err));
    BOOST_CHECK(!err.empty());
}

#ifdef _WIN32

BOOST_AUTO_TEST_CASE(test_unicode_environment) {
    wchar_t marker[2];
    if (GetEnvironmentVariableW(L"STDC_POPEN_UNICODE_CHILD", marker, 2) != 0) {
        wchar_t value[32];
        DWORD size = GetEnvironmentVariableW(L"\u53d8\u91cf", value, DWORD(std::size(value)));
        BOOST_REQUIRE(size > 0 && size < std::size(value));
        BOOST_CHECK(std::wstring(value, size) == L"\u503c\u6d4b\u8bd5");
        return;
    }

    Popen p;
    std::string err;
    p.args({
               system::application_path().string(),
               "--run_test=test_popen/test_unicode_environment", "--log_level=nothing"
    })
        .env({{"STDC_POPEN_UNICODE_CHILD", "1"}, {"\u53d8\u91cf", "\u503c\u6d4b\u8bd5"}});
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    BOOST_REQUIRE(p.wait(Timeout));
    BOOST_CHECK_EQUAL(*p.returncode(), 0);
}

#endif

#ifndef _WIN32

// The child's environment is replaced, not merged.
BOOST_AUTO_TEST_CASE(test_env) {
    Popen p;
    std::string err;
    p.args({
               "/bin/sh", "-c", "echo $FOO"
    })
        .env({{"FOO", "bar"}, {"PATH", "/bin:/usr/bin"}})
        .stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    auto [out, errout] = p.communicate({}, Timeout);
    BOOST_CHECK_EQUAL(first_line(out), "bar");
}

BOOST_AUTO_TEST_CASE(test_replaced_env_does_not_use_parent_path) {
    Popen p;
    std::string err;
    p.args({
               "sh", "-c", "exit 0"
    })
        .env({{"FOO", "bar"}});
    BOOST_CHECK(!p.start(&err));
    BOOST_CHECK(p.error_code() == std::make_error_code(std::errc::no_such_file_or_directory));
}

BOOST_AUTO_TEST_CASE(test_user_name_is_owned) {
    struct passwd *pw = getpwuid(getuid());
    BOOST_REQUIRE(pw != nullptr);

    Popen p;
    std::string err;
    std::string name = pw->pw_name;
    p.args(shell_args(ExitZero)).user(name.c_str());
    name.assign(name.size(), 'x');
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    BOOST_REQUIRE(p.wait(Timeout));
    BOOST_CHECK_EQUAL(*p.returncode(), 0);
}

BOOST_AUTO_TEST_CASE(test_shell) {
    Popen p;
    std::string err;
    p.args({"echo shelled"}).shell(true).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    auto [out, errout] = p.communicate({}, Timeout);
    BOOST_CHECK_EQUAL(first_line(out), "shelled");
}

// A signal death is reported as the negated signal number, as in Python.
BOOST_AUTO_TEST_CASE(test_signal_returncode) {
    {
        Popen p;
        std::string err;
        p.args(FilterX).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_CHECK(p.kill());
        BOOST_REQUIRE(p.wait(Timeout));
        BOOST_CHECK_EQUAL(*p.returncode(), -SIGKILL);
    }
    {
        Popen p;
        std::string err;
        p.args(FilterX).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_CHECK(p.terminate());
        BOOST_REQUIRE(p.wait(Timeout));
        BOOST_CHECK_EQUAL(*p.returncode(), -SIGTERM);
    }
}

// Every descriptor a run opens has to come back, or a long-lived program runs out of them.
BOOST_AUTO_TEST_CASE(test_no_fd_leak) {
    const auto &open_fd_count = []() {
        DIR *dir = opendir("/proc/self/fd");
        if (!dir) {
            return -1;
        }
        int count = 0;
        while (readdir(dir)) {
            count++;
        }
        closedir(dir);
        return count;
    };

    // The first run warms up whatever the library allocates once.
    for (int i = 0; i < 2; i++) {
        Popen p;
        std::string err;
        p.args(FilterX).stdin_(Popen::PIPE).stdout_(Popen::PIPE).stderr_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        std::ignore = p.communicate("xyz\n", Timeout);
    }

    int before = open_fd_count();
    BOOST_REQUIRE(before > 0);
    for (int i = 0; i < 20; i++) {
        Popen p;
        std::string err;
        p.args(FilterX).stdin_(Popen::PIPE).stdout_(Popen::PIPE).stderr_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        std::ignore = p.communicate("xyz\n", Timeout);
    }
    BOOST_CHECK_EQUAL(open_fd_count(), before);
}

// close_fds is on by default, so nothing of ours reaches the child but the standard streams.
BOOST_AUTO_TEST_CASE(test_close_fds) {
    Popen p;
    std::string err;
    p.args({"/bin/sh", "-c", "ls /proc/self/fd | wc -l"}).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    auto [out, errout] = p.communicate({}, Timeout);
    // 0, 1, 2 and the descriptor ls itself opened on the directory
    BOOST_CHECK_LE(std::stoi(first_line(out)), 5);
}

// preexec_fn runs in the child, after the pipes are in place and before exec.
BOOST_AUTO_TEST_CASE(test_preexec_fn) {
    Popen p;
    std::string err;
    p.args({"pwd"}).stdout_(Popen::PIPE).preexec_fn([] { std::ignore = chdir("/usr"); });
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    auto [out, errout] = p.communicate({}, Timeout);
    BOOST_CHECK_EQUAL(first_line(out), "/usr");
}

// A descriptor named in pass_fds survives exec. Without it, close_fds takes it away.
BOOST_AUTO_TEST_CASE(test_pass_fds) {
    // A pipe holding a known line, which the child reads by descriptor number.
    const auto &filled_pipe = [](int fds[2]) {
        BOOST_REQUIRE_EQUAL(pipe(fds), 0);
        const char *msg = "kept\n";
        std::ignore = write(fds[1], msg, std::strlen(msg));
        close(fds[1]);
    };

    {
        int fds[2];
        filled_pipe(fds);
        char script[64];
        std::snprintf(script, sizeof(script), "cat <&%d", fds[0]);

        Popen p;
        std::string err;
        p.args({"/bin/sh", "-c", script}).pass_fds({fds[0]}).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        close(fds[0]);
        BOOST_CHECK_EQUAL(first_line(out), "kept");
    }

    {
        int fds[2];
        filled_pipe(fds);
        char script[64];
        std::snprintf(script, sizeof(script), "cat <&%d", fds[0]);

        Popen p;
        std::string err;
        p.args({"/bin/sh", "-c", script}).stdout_(Popen::PIPE).stderr_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        close(fds[0]);
        BOOST_CHECK(out.empty());
        BOOST_CHECK(!errout.empty());
    }

    // close_fds(false) hands the child everything we have open, so the same read works.
    {
        int fds[2];
        filled_pipe(fds);
        char script[64];
        std::snprintf(script, sizeof(script), "cat <&%d", fds[0]);

        Popen p;
        std::string err;
        p.args({"/bin/sh", "-c", script}).close_fds(false).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        close(fds[0]);
        BOOST_CHECK_EQUAL(first_line(out), "kept");
    }
}

BOOST_AUTO_TEST_CASE(test_umask) {
    Popen p;
    std::string err;
    p.args({"/bin/sh", "-c", "umask"}).umask(0077).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    auto [out, errout] = p.communicate({}, Timeout);
    BOOST_CHECK_EQUAL(first_line(out), "0077");
}

// Both of these are visible from here, so the child does not have to report them.
BOOST_AUTO_TEST_CASE(test_session_and_process_group) {
    {
        Popen p;
        std::string err;
        p.args({"cat"}).stdin_(Popen::PIPE).start_new_session(true);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_CHECK_EQUAL(getsid(p.pid()), p.pid());
        p.stdin_().close();
        BOOST_REQUIRE(p.wait(Timeout));
    }
    {
        Popen p;
        std::string err;
        p.args({"cat"}).stdin_(Popen::PIPE).process_group(0);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_CHECK_EQUAL(getpgid(p.pid()), p.pid());
        p.stdin_().close();
        BOOST_REQUIRE(p.wait(Timeout));
    }
}

#  ifdef F_GETPIPE_SZ
BOOST_AUTO_TEST_CASE(test_pipesize) {
    Popen p;
    std::string err;
    p.args({"cat"}).stdin_(Popen::PIPE).pipesize(1 << 17);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    // The kernel may round up, so this is a floor rather than the exact figure.
    BOOST_CHECK_GE(fcntl(fileno(p.stdin_().file()), F_GETPIPE_SZ), 1 << 17);
    p.stdin_().close();
    BOOST_REQUIRE(p.wait(Timeout));
}
#  endif

// restore_signals puts the dispositions the parent changed back to their defaults, so a child
// does not inherit an ignored signal it never asked for.
BOOST_AUTO_TEST_CASE(test_restore_signals) {
    struct IgnoreSigpipe {
        IgnoreSigpipe() : prev(signal(SIGPIPE, SIG_IGN)) {
        }
        ~IgnoreSigpipe() {
            signal(SIGPIPE, prev);
        }
        void (*prev)(int);
    } ignore;

    const char *script = "kill -PIPE $$; echo survived";

    {
        Popen p;
        std::string err;
        p.args({"/bin/sh", "-c", script}).restore_signals(true).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        BOOST_CHECK(out.empty());
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), -SIGPIPE);
    }

    {
        Popen p;
        std::string err;
        p.args({"/bin/sh", "-c", script}).restore_signals(false).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        BOOST_CHECK_EQUAL(first_line(out), "survived");
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }
}

// A name that no passwd entry matches fails before anything is forked.
BOOST_AUTO_TEST_CASE(test_unknown_user_name) {
    Popen p;
    std::string err;
    p.args(shell_args(ExitZero)).user("no_such_user_9f3a");
    BOOST_CHECK(!p.start(&err));
    BOOST_CHECK(!err.empty());
    BOOST_CHECK_EQUAL(p.pid(), -1);
}

// Changing credentials needs privilege, so which half of this runs depends on who we are. Under
// a normal user the point is that the failure is orderly: start() says no, and the child that
// got as far as fork is reaped rather than left behind.
BOOST_AUTO_TEST_CASE(test_user_and_groups) {
    const bool privileged = geteuid() == 0;

    const auto &check = [&](Popen &p, const char *expected) {
        std::string err;
        bool started = p.start(&err);
        BOOST_REQUIRE_EQUAL(started, privileged);
        if (!started) {
            BOOST_CHECK(!err.empty());
            BOOST_CHECK(p.returncode());
            return;
        }
        auto [out, errout] = p.communicate({}, Timeout);
        BOOST_CHECK_EQUAL(first_line(out), expected);
    };

    {
        Popen p;
        p.args({"id", "-u"}).user(65534).stdout_(Popen::PIPE);
        check(p, "65534");
    }
    {
        Popen p;
        p.args({"id", "-un"}).user("nobody").stdout_(Popen::PIPE);
        check(p, "nobody");
    }
    {
        Popen p;
        p.args({"id", "-g"}).group(65534).stdout_(Popen::PIPE);
        check(p, "65534");
    }
    {
        // setgroups replaces the supplementary list, leaving the primary group beside it.
        Popen p;
        p.args({"id", "-G"}).extra_groups({65534}).stdout_(Popen::PIPE);
        check(p, "0 65534");
    }
}

#endif // !_WIN32

#ifdef _WIN32

// An argument starting with a quote used to walk a size_t counter past zero and throw
// std::out_of_range out of start().
BOOST_AUTO_TEST_CASE(test_argument_quoting) {
    const std::vector<std::string> tricky = {
        "plain", "a b", "a\"b", "\"lead", "trail\"", "back\\slash", "end\\", "",
    };
    for (const auto &arg : tricky) {
        Popen p;
        std::string err;
        p.args({"cmd", "/c", "exit 0", arg});
        BOOST_CHECK_MESSAGE(p.start(&err), "start failed for [" + arg + "]: " + err);
        if (p.returncode() || p.pid() > 0) {
            p.wait(Timeout);
        }
    }
}

#endif // _WIN32

BOOST_AUTO_TEST_CASE(test_devnull_and_inherit) {
    // DEVNULL swallows the output
    {
        Popen p;
        std::string err;
        p.args(shell_args(EchoHello)).stdout_(Popen::DEVNULL);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_REQUIRE(p.wait(Timeout));
        BOOST_CHECK(!p.stdout_().is_open());
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }

    // no redirection at all: the child inherits ours
    {
        Popen p;
        std::string err;
        p.args(shell_args(ExitZero));
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_REQUIRE(p.wait(Timeout));
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }
}

// The pipes are C++ streams, so the usual stream vocabulary works on them directly and nobody
// has to reach for a platform-specific adapter to get there.
BOOST_AUTO_TEST_CASE(test_stream_interface) {
    // reading with getline
    {
        Popen p;
        std::string err;
        p.args(shell_args(EchoThree)).stdout_(Popen::PIPE).text(true);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(p.stdout_(), line)) {
            lines.push_back(first_line(line));
        }
        BOOST_REQUIRE_EQUAL(lines.size(), 3u);
        BOOST_CHECK_EQUAL(lines[0], "one");
        BOOST_CHECK_EQUAL(lines[2], "three");
        BOOST_REQUIRE(p.wait(Timeout));
    }

    // writing with operator<<, then closing to release the child
    {
        Popen p;
        std::string err;
        p.args({"sort"}).stdin_(Popen::PIPE).stdout_(Popen::PIPE).text(true);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);

        p.stdin_() << "banana\ncherry\napple\n" << std::flush;
        p.stdin_().close();

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(p.stdout_(), line)) {
            auto trimmed = first_line(line);
            if (!trimmed.empty()) {
                lines.push_back(trimmed);
            }
        }
        BOOST_REQUIRE_EQUAL(lines.size(), 3u);
        BOOST_CHECK_EQUAL(lines[0], "apple");
        BOOST_CHECK_EQUAL(lines[1], "banana");
        BOOST_CHECK_EQUAL(lines[2], "cherry");
        BOOST_REQUIRE(p.wait(Timeout));
    }

    // close() is idempotent, and a stream that was never opened is simply not open
    {
        Popen p;
        std::string err;
        p.args(shell_args(ExitZero)).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);

        BOOST_CHECK(p.stdout_().is_open());
        BOOST_CHECK(!p.stdin_().is_open());
        BOOST_CHECK(p.stdin_().file() == nullptr);

        p.stdout_().close();
        BOOST_CHECK(!p.stdout_().is_open());
        p.stdout_().close();
        p.stdout_().close();
        BOOST_CHECK(p.stdout_().file() == nullptr);

        BOOST_REQUIRE(p.wait(Timeout));
    }

    // file() hands the same pipe to the C interfaces that only take a FILE *
    {
        Popen p;
        std::string err;
        p.args(shell_args(EchoHello)).stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);

        FILE *raw = p.stdout_().file();
        BOOST_REQUIRE(raw != nullptr);
        char buf[128] = {};
        size_t n = std::fread(buf, 1, sizeof(buf) - 1, raw);
        BOOST_CHECK_GT(n, 0u);
        BOOST_CHECK_EQUAL(first_line(buf), "hello");
        BOOST_REQUIRE(p.wait(Timeout));
    }
}

BOOST_AUTO_TEST_CASE(test_kill) {
    Popen p;
    std::string err;
    p.args(FilterX).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    BOOST_CHECK(!p.poll());

    BOOST_CHECK(p.kill());
    BOOST_REQUIRE(p.wait(Timeout));
    BOOST_REQUIRE(p.returncode());
    BOOST_CHECK(*p.returncode() != 0);

    // killing an already-dead process is a no-op, not a failure
    BOOST_CHECK(p.kill());
}

// A Popen owns its child by default and takes it down on the way out. A process configured as
// detached before start is independent and cannot be waited or controlled through this object.
BOOST_AUTO_TEST_CASE(test_detached) {
    // the default: destroying the object ends the child
    {
        int pid = -1;
        {
            Popen p;
            std::string err;
            p.args(shell_args(SleepLong));
            BOOST_REQUIRE_MESSAGE(p.start(&err), err);
            pid = p.pid();
            BOOST_REQUIRE(pid > 0);
            BOOST_CHECK(process_alive(pid));
            BOOST_CHECK(!p.detached());
        }
        BOOST_CHECK(!process_alive(pid));
    }

    // detached: it outlives us
    {
        int pid = -1;
        {
            Popen p;
            std::string err;
            p.args(shell_args(SleepLong)).detached(true);
            BOOST_REQUIRE_MESSAGE(p.start(&err), err);
            BOOST_CHECK(p.detached());
            pid = p.pid();
            BOOST_REQUIRE(pid > 0);
        }
        BOOST_CHECK(process_alive(pid));
        hard_kill(pid);
    }

    // Detachment changes how the process is created, so it cannot be enabled after start().
    {
        int pid = -1;
        {
            Popen p;
            std::string err;
            p.args(shell_args(SleepLong));
            BOOST_REQUIRE_MESSAGE(p.start(&err), err);
            pid = p.pid();
            p.detached(true);
            BOOST_CHECK(!p.detached());
        }
        BOOST_CHECK(!process_alive(pid));
    }

    // The final detached process is deliberately no longer controllable by Popen.
    {
        Popen p;
        std::string err;
        p.args(shell_args(SleepLong)).detached(true);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        int pid = p.pid();
        BOOST_REQUIRE(pid > 0);
        BOOST_CHECK(!p.wait(0));
        BOOST_CHECK(p.error_code() == std::make_error_code(std::errc::operation_not_supported));
        BOOST_CHECK(!p.kill());
        BOOST_CHECK(p.error_code() == std::make_error_code(std::errc::operation_not_supported));

#ifndef _WIN32
        // The double-forked process belongs to init (or a configured child subreaper), not us.
        int status = 0;
        errno = 0;
        BOOST_CHECK_EQUAL(waitpid(pid, &status, WNOHANG), -1);
        BOOST_CHECK_EQUAL(errno, ECHILD);
#endif
        hard_kill(pid);
    }

    // Pipes need an owner to drain/close them, so detached mode rejects them.
    {
        Popen p;
        std::string err;
        p.args(shell_args(ExitZero)).stdout_(Popen::PIPE).detached(true);
        BOOST_CHECK(!p.start(&err));
        BOOST_CHECK(p.error_code() == std::make_error_code(std::errc::invalid_argument));
    }
}

BOOST_AUTO_TEST_SUITE_END()
