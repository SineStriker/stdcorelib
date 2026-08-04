// SPDX-License-Identifier: MIT

#include <cstdio>
#include <string>

#include <stdcorelib/support/logging.h>

#include <boost/test/unit_test.hpp>

#ifdef _WIN32
#  include <io.h>
#  define stdc_dup    _dup
#  define stdc_dup2   _dup2
#  define stdc_close  _close
#  define stdc_fileno _fileno
#  define NullDevice  "NUL"
#else
#  include <unistd.h>
#  define stdc_dup    dup
#  define stdc_dup2   dup2
#  define stdc_close  close
#  define stdc_fileno fileno
#  define NullDevice  "/dev/null"
#endif

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_logging)

namespace {

    // The rules, the installed filter and the callback all live in a process-global registry, so
    // each case sets up its own state and a guard puts the defaults back even if an assertion
    // throws part way through.
    void setRules(const std::string &rules) {
        LogCategory::defaultCategory().setFilterRules(rules);
    }

    struct LoggingGuard {
        ~LoggingGuard() {
            LogCategory::setLogFilter(nullptr);
            LogCategory::defaultCategory().setFilterRules("");
        }
    };

    bool allEnabled(const LogCategory &c) {
        for (int level = Logger::Trace; level <= Logger::Fatal; ++level) {
            if (!c.isLevelEnabled(level)) {
                return false;
            }
        }
        return true;
    }

    bool allDisabled(const LogCategory &c) {
        for (int level = Logger::Trace; level <= Logger::Fatal; ++level) {
            if (c.isLevelEnabled(level)) {
                return false;
            }
        }
        return true;
    }

    int g_emitCount = 0;
    int g_lastLevel = 0;

    void captureSink(int level, const LogContext &, const std::string_view &) {
        ++g_emitCount;
        g_lastLevel = level;
    }

    // Redirects stdout and stderr into a scratch file for as long as it lives, and hands back
    // what was written. The default sink writes to them directly, so this is the only way to see
    // what it produced.
    class CapturedOutput {
    public:
        CapturedOutput() {
            std::fflush(stdout);
            std::fflush(stderr);
            _sink = std::tmpfile();
            _out = stdc_dup(stdc_fileno(stdout));
            _err = stdc_dup(stdc_fileno(stderr));
            if (_sink) {
                stdc_dup2(stdc_fileno(_sink), stdc_fileno(stdout));
                stdc_dup2(stdc_fileno(_sink), stdc_fileno(stderr));
            }
        }

        ~CapturedOutput() {
            std::fflush(stdout);
            std::fflush(stderr);
            stdc_dup2(_out, stdc_fileno(stdout));
            stdc_dup2(_err, stdc_fileno(stderr));
            stdc_close(_out);
            stdc_close(_err);
            if (_sink) {
                std::fclose(_sink);
            }
        }

        std::string contents() const {
            std::fflush(stdout);
            std::fflush(stderr);
            if (!_sink) {
                return {};
            }
            std::fseek(_sink, 0, SEEK_SET);
            std::string text;
            char buf[1024];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof(buf), _sink)) > 0) {
                text.append(buf, n);
            }
            return text;
        }

    private:
        FILE *_sink = nullptr;
        int _out = -1;
        int _err = -1;
    };

}

BOOST_AUTO_TEST_CASE(test_all_levels_enabled_without_rules) {
    LoggingGuard guard;
    LogCategory c("stdc.test.fresh");
    BOOST_CHECK(allEnabled(c));
}

BOOST_AUTO_TEST_CASE(test_category_match_modes) {
    // an exact pattern matches the identical name and nothing else
    {
        LoggingGuard guard;
        LogCategory hit("stdc.exact");
        LogCategory miss("stdc.exact.sub");
        setRules("stdc.exact=false");
        BOOST_CHECK(allDisabled(hit));
        BOOST_CHECK(allEnabled(miss));
    }

    // a trailing wildcard matches the subtree, and a category built after the rule picks it up
    {
        LoggingGuard guard;
        setRules("stdc.io.*=false");
        LogCategory in("stdc.io.socket");
        LogCategory out("stdc.gfx");
        BOOST_CHECK(allDisabled(in));
        BOOST_CHECK(allEnabled(out));
    }

    // a leading wildcard matches the tail
    {
        LoggingGuard guard;
        setRules("*.io=false");
        LogCategory in("stdc.io");
        LogCategory out("stdc.ioext");
        BOOST_CHECK(allDisabled(in));
        BOOST_CHECK(allEnabled(out));
    }

    // wildcards at both ends match a substring
    {
        LoggingGuard guard;
        setRules("*io*=false");
        LogCategory in("stdc.iostream.x");
        LogCategory out("stdc.gfx");
        BOOST_CHECK(allDisabled(in));
        BOOST_CHECK(allEnabled(out));
    }

    // a bare star matches everything
    {
        LoggingGuard guard;
        setRules("*=false");
        LogCategory a("anything");
        LogCategory b("stdc.deep.name");
        BOOST_CHECK(allDisabled(a));
        BOOST_CHECK(allDisabled(b));
    }
}

BOOST_AUTO_TEST_CASE(test_level_selectors) {
    {
        LoggingGuard guard;
        LogCategory c("stdc.level");
        setRules("stdc.level.debug=false");
        BOOST_CHECK(!c.isLevelEnabled(Logger::Debug));
        BOOST_CHECK(c.isLevelEnabled(Logger::Warning));
        BOOST_CHECK(c.isLevelEnabled(Logger::Trace));
    }

    // every level has a token, and only the named one is affected
    {
        LoggingGuard guard;
        const struct {
            const char *token;
            int level;
        } cases[] = {
            {"trace",       Logger::Trace      },
            {"debug",       Logger::Debug      },
            {"success",     Logger::Success    },
            {"info",        Logger::Information},
            {"information", Logger::Information},
            {"warning",     Logger::Warning    },
            {"critical",    Logger::Critical   },
            {"fatal",       Logger::Fatal      },
        };
        for (const auto &tc : cases) {
            LogCategory c("stdc.tok");
            setRules(std::string("stdc.tok.") + tc.token + "=false");
            BOOST_CHECK_MESSAGE(!c.isLevelEnabled(tc.level), tc.token);
            int other = tc.level == Logger::Warning ? Logger::Trace : Logger::Warning;
            BOOST_CHECK_MESSAGE(c.isLevelEnabled(other), tc.token);
            setRules("");
        }
    }
}

BOOST_AUTO_TEST_CASE(test_rule_order_and_separators) {
    // disable everything, then put warnings back
    {
        LoggingGuard guard;
        LogCategory c("stdc.order");
        setRules("stdc.order=false\nstdc.order.warning=true");
        BOOST_CHECK(c.isLevelEnabled(Logger::Warning));
        BOOST_CHECK(!c.isLevelEnabled(Logger::Debug));
        BOOST_CHECK(!c.isLevelEnabled(Logger::Critical));
    }

    // '#' starts a comment and ';' separates rules the same way a newline does
    {
        LoggingGuard guard;
        LogCategory c("anything.at.all");
        setRules("# turn everything off\n*=false ; anything.at.all.info=true");
        BOOST_CHECK(c.isLevelEnabled(Logger::Information));
        BOOST_CHECK(!c.isLevelEnabled(Logger::Debug));
    }
}

BOOST_AUTO_TEST_CASE(test_boolean_spellings) {
    LoggingGuard guard;
    LogCategory tTrue("stdc.btrue");
    LogCategory tOne("stdc.bone");
    LogCategory tUpper("stdc.bupper");
    LogCategory fFalse("stdc.bfalse");
    LogCategory fZero("stdc.bzero");

    setRules("*=false\n"
             "stdc.btrue=true\n"
             "stdc.bone=1\n"
             "stdc.bupper=TRUE\n"
             "stdc.bfalse=false\n"
             "stdc.bzero=0\n");

    BOOST_CHECK(tTrue.isLevelEnabled(Logger::Warning));
    BOOST_CHECK(tOne.isLevelEnabled(Logger::Warning));
    BOOST_CHECK(tUpper.isLevelEnabled(Logger::Warning));
    BOOST_CHECK(!fFalse.isLevelEnabled(Logger::Warning));
    BOOST_CHECK(!fZero.isLevelEnabled(Logger::Warning));
}

BOOST_AUTO_TEST_CASE(test_malformed_rules_are_ignored) {
    // no '=', a wildcard in the middle, and a value that is not a boolean
    {
        LoggingGuard guard;
        LogCategory c("stdc.bad");
        setRules("stdc.bad\n"
                 "st*dc.bad=false\n"
                 "stdc.bad=maybe");
        BOOST_CHECK(allEnabled(c));
    }

    // an unknown trailing token is part of the category name, not a level
    {
        LoggingGuard guard;
        LogCategory exact("stdc.x.bogus");
        LogCategory parent("stdc.x");
        setRules("stdc.x.bogus=false");
        BOOST_CHECK(allDisabled(exact));
        BOOST_CHECK(allEnabled(parent));
    }
}

BOOST_AUTO_TEST_CASE(test_filter_rules_roundtrip) {
    LoggingGuard guard;
    LogCategory c("stdc.reset");

    setRules("stdc.reset=false");
    BOOST_CHECK_EQUAL(LogCategory::filterRules(), "stdc.reset=false");
    BOOST_CHECK(!c.isLevelEnabled(Logger::Warning));

    setRules("");
    BOOST_CHECK(allEnabled(c));
}

// Mirrors Qt, where installing a filter takes the rules out of the picture.
BOOST_AUTO_TEST_CASE(test_custom_filter_replaces_the_default) {
    LoggingGuard guard;

    LogCategory::setLogFilter([](LogCategory *cat) {
        for (int level = Logger::Trace; level <= Logger::Fatal; ++level) {
            cat->setLevelEnabled(level, false);
        }
    });

    LogCategory c("stdc.custom");
    BOOST_CHECK(allDisabled(c));

    // only the default filter consults the rules, so this one has no effect
    setRules("stdc.custom=true");
    BOOST_CHECK(allDisabled(c));

    LogCategory::setLogFilter(nullptr);
    BOOST_CHECK(c.isLevelEnabled(Logger::Warning));
}

// The rules have to gate emission, not just the isLevelEnabled() flags.
BOOST_AUTO_TEST_CASE(test_disabled_level_never_reaches_the_callback) {
    LoggingGuard guard;
    auto prev = Logger::logCallback();
    Logger::setLogCallback(captureSink);

    LogCategory c("stdc.emit");
    setRules("stdc.emit.debug=false");

    g_emitCount = 0;
    g_lastLevel = 0;
    c.log<Logger::Debug>(__FILE__, __LINE__, __FUNCTION__, "should be suppressed");
    int afterDebug = g_emitCount;
    c.log<Logger::Warning>(__FILE__, __LINE__, __FUNCTION__, "should be emitted");
    int afterWarning = g_emitCount;
    int lastLevel = g_lastLevel;

    // restored before asserting, so a failure cannot leave the sink installed
    Logger::setLogCallback(prev);

    BOOST_CHECK_EQUAL(afterDebug, 0);
    BOOST_CHECK_EQUAL(afterWarning, 1);
    BOOST_CHECK_EQUAL(lastLevel, int(Logger::Warning));
}

// The macros resolve an in-scope category through stdcGetLogCategory(), and fall back to the
// default one when there is none.
BOOST_AUTO_TEST_CASE(test_macros) {
    LoggingGuard guard;
    auto prev = Logger::logCallback();
    Logger::setLogCallback(captureSink);

    LogCategory lc("stdc.macro");
    setRules("stdc.macro=false\nstdc.macro.warning=true");

    g_emitCount = 0;
    g_lastLevel = 0;
    lc.stdcDebug("suppressed");
    lc.stdcWarning("emitted with arg: %1", 42);
    lc.stdcWarningF("emitted with arg: %d", 42);
    int afterCategory = g_emitCount;

    setRules("");
    stdcWarning("through the default category");
    int afterDefault = g_emitCount;

    Logger::setLogCallback(prev);

    BOOST_CHECK_EQUAL(afterCategory, 2);
    BOOST_CHECK_EQUAL(afterDefault, 3);
    BOOST_CHECK_EQUAL(g_lastLevel, int(Logger::Warning));
}

// Every case above replaces the sink, so the built-in one had never been run. It dropped
// Information, which is 4 against Success's 3 and so clears the level gate and reaches the
// switch, where it had no case of its own. A debug build aborted on the assert there, a release
// build compiled the assert away and lost the message quietly, which is why surviving the call
// is not enough to check.
BOOST_AUTO_TEST_CASE(default_sink_emits_every_level_at_or_above_success) {
    LoggingGuard guard;
    Logger::setLogCallback(nullptr); // put the built-in one back

    std::string text;
    {
        CapturedOutput captured;
        LogContext context(__FILE__, __LINE__, __FUNCTION__, "stdc.sink");
        Logger(context).print(Logger::Trace, "trace-line");
        Logger(context).print(Logger::Debug, "debug-line");
        Logger(context).print(Logger::Success, "success-line");
        Logger(context).print(Logger::Information, "information-line");
        Logger(context).print(Logger::Warning, "warning-line");
        Logger(context).print(Logger::Critical, "critical-line");
        Logger(context).print(Logger::Fatal, "fatal-line");
        text = captured.contents();
    }

    // below the gate, so deliberately absent
    BOOST_CHECK(text.find("trace-line") == std::string::npos);
    BOOST_CHECK(text.find("debug-line") == std::string::npos);

    // at or above it, so every one of them has to appear
    BOOST_CHECK_MESSAGE(text.find("success-line") != std::string::npos, text);
    BOOST_CHECK_MESSAGE(text.find("information-line") != std::string::npos, text);
    BOOST_CHECK_MESSAGE(text.find("warning-line") != std::string::npos, text);
    BOOST_CHECK_MESSAGE(text.find("critical-line") != std::string::npos, text);
    BOOST_CHECK_MESSAGE(text.find("fatal-line") != std::string::npos, text);
}

// print() takes an int rather than a Level, so a caller can invent one. That must neither abort
// nor swallow the message.
BOOST_AUTO_TEST_CASE(default_sink_takes_a_level_outside_the_enum) {
    LoggingGuard guard;
    Logger::setLogCallback(nullptr);

    std::string text;
    {
        CapturedOutput captured;
        LogContext context(__FILE__, __LINE__, __FUNCTION__, "stdc.sink");
        Logger(context).print(42, "invented-level");
        Logger(context).print(-1, "negative-level"); // below the gate, dropped
        text = captured.contents();
    }

    BOOST_CHECK_MESSAGE(text.find("invented-level") != std::string::npos, text);
    BOOST_CHECK(text.find("negative-level") == std::string::npos);
}

// nullptr means the built-in sink, the way it already does for setLogFilter(). Assigning it raw
// would leave a null pointer for the next record to call through.
BOOST_AUTO_TEST_CASE(null_callback_restores_the_default) {
    LoggingGuard guard;
    auto original = Logger::logCallback();

    Logger::setLogCallback(captureSink);
    BOOST_CHECK(Logger::logCallback() == captureSink);

    Logger::setLogCallback(nullptr);
    BOOST_CHECK(Logger::logCallback() != nullptr);
    BOOST_CHECK(Logger::logCallback() == original);

    std::string text;
    {
        CapturedOutput captured;
        stdcWarning("built-in-sink-reached");
        text = captured.contents();
    }
    BOOST_CHECK_MESSAGE(text.find("built-in-sink-reached") != std::string::npos, text);
}

BOOST_AUTO_TEST_SUITE_END()