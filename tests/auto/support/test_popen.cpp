#include <string>
#include <vector>

#include <stdcorelib/support/popen.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

// The POSIX side of Popen is still unfinished (_execute_child and communicate() are stubs), so
// there is nothing to assert there yet.
#ifdef _WIN32

BOOST_AUTO_TEST_SUITE(test_popen)

namespace {

    // Every wait here is bounded, so a regression hangs the one case rather than the whole run.
    constexpr int Timeout = 15000;

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
        p.args({"cmd", "/c", "exit 0"});
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
        p.args({"cmd", "/c", "exit 3"});
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

// wait() used to close the pipes along with the process handle, which threw away the output
// before anyone could read it.
BOOST_AUTO_TEST_CASE(test_output_survives_wait) {
    Popen p;
    std::string err;
    p.args({"cmd", "/c", "echo hello"}).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    BOOST_REQUIRE(p.wait(Timeout));

    FILE *out = p.stdout_();
    BOOST_REQUIRE(out != nullptr);
    char buf[128] = {};
    size_t n = std::fread(buf, 1, sizeof(buf) - 1, out);
    BOOST_CHECK_GT(n, 0u);
    BOOST_CHECK_EQUAL(first_line(buf), "hello");
}

BOOST_AUTO_TEST_CASE(test_communicate) {
    // stdout only
    {
        Popen p;
        std::string err;
        p.args({"cmd", "/c", "echo hello"}).stdout_(Popen::PIPE);
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
        p.args({"cmd", "/c", "echo out& echo err 1>&2"}).stdout_(Popen::PIPE).stderr_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        BOOST_CHECK_EQUAL(first_line(out), "out");
        BOOST_CHECK_EQUAL(first_line(errout), "err");
    }

    // stderr folded into stdout
    {
        Popen p;
        std::string err;
        p.args({"cmd", "/c", "echo err 1>&2"}).stdout_(Popen::PIPE).stderr_(Popen::STDOUT);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        BOOST_CHECK_EQUAL(first_line(out), "err");
        BOOST_CHECK(errout.empty());
    }

    // input is written and the pipe closed, so a filter can finish
    {
        Popen p;
        std::string err;
        p.args({"findstr", "x"}).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
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
        p.args({"cmd", "/c", "for /L %i in (1,1,5000) do @echo 0123456789012345678901234567890123"})
            .stdout_(Popen::PIPE);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        auto [out, errout] = p.communicate({}, Timeout);
        BOOST_CHECK_GT(out.size(), 100000u);
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }
}

// Without a way to close this side of the pipe, a child reading to end of input never returns.
BOOST_AUTO_TEST_CASE(test_close_stdin_ends_input) {
    Popen p;
    std::string err;
    p.args({"findstr", "x"}).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);

    FILE *in = p.stdin_();
    BOOST_REQUIRE(in != nullptr);
    std::fputs("xyz\n", in);
    std::fflush(in);

    // still running: nothing has told it the input is over
    BOOST_CHECK(!p.wait(200));
    BOOST_CHECK(!p.returncode());

    p.close_stdin();
    BOOST_CHECK(p.stdin_() == nullptr);

    BOOST_REQUIRE(p.wait(Timeout));
    BOOST_REQUIRE(p.returncode());
    BOOST_CHECK_EQUAL(*p.returncode(), 0);
}

// poll() returning false means "not finished", which is not an error, and it must not pick up
// whatever GetLastError happened to be holding.
BOOST_AUTO_TEST_CASE(test_poll) {
    Popen p;
    std::string err;
    p.args({"findstr", "x"}).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);

    BOOST_CHECK(!p.poll());
    BOOST_CHECK(!p.returncode());
    BOOST_CHECK_EQUAL(p.error_code().value(), 0);

    p.close_stdin();
    BOOST_REQUIRE(p.wait(Timeout));

    // once it has exited, poll() says so and keeps saying so
    BOOST_CHECK(p.poll());
    BOOST_REQUIRE(p.returncode());
    BOOST_CHECK(p.poll());
    BOOST_CHECK_EQUAL(p.error_code().value(), 0);
}

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

BOOST_AUTO_TEST_CASE(test_devnull_and_fd) {
    // DEVNULL swallows the output
    {
        Popen p;
        std::string err;
        p.args({"cmd", "/c", "echo hello"}).stdout_(Popen::DEVNULL);
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_REQUIRE(p.wait(Timeout));
        BOOST_CHECK(p.stdout_() == nullptr);
        BOOST_REQUIRE(p.returncode());
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }

    // no redirection at all: the child inherits ours
    {
        Popen p;
        std::string err;
        p.args({"cmd", "/c", "exit 0"});
        BOOST_REQUIRE_MESSAGE(p.start(&err), err);
        BOOST_REQUIRE(p.wait(Timeout));
        BOOST_CHECK_EQUAL(*p.returncode(), 0);
    }
}

BOOST_AUTO_TEST_CASE(test_kill) {
    Popen p;
    std::string err;
    p.args({"findstr", "x"}).stdin_(Popen::PIPE).stdout_(Popen::PIPE);
    BOOST_REQUIRE_MESSAGE(p.start(&err), err);
    BOOST_CHECK(!p.poll());

    BOOST_CHECK(p.kill());
    BOOST_REQUIRE(p.wait(Timeout));
    BOOST_REQUIRE(p.returncode());
    BOOST_CHECK(*p.returncode() != 0);

    // killing an already-dead process is a no-op, not a failure
    BOOST_CHECK(p.kill());
}

BOOST_AUTO_TEST_SUITE_END()

#endif // _WIN32
