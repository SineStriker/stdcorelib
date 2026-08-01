#include <stdcorelib/str.h>

#include <cstdarg>
#include <map>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

namespace fs = std::filesystem;

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_str)

BOOST_AUTO_TEST_CASE(test_split) {
    // string_view overload yields views into the source
    {
        auto parts = str::split("a,b,c", ",");
        BOOST_REQUIRE_EQUAL(parts.size(), 3u);
        BOOST_CHECK_EQUAL(parts[0], "a");
        BOOST_CHECK_EQUAL(parts[1], "b");
        BOOST_CHECK_EQUAL(parts[2], "c");
    }

    // empty fields are kept
    {
        auto parts = str::split("a,,b", ",");
        BOOST_REQUIRE_EQUAL(parts.size(), 3u);
        BOOST_CHECK_EQUAL(parts[1], "");
    }
    {
        auto parts = str::split("a,b,", ",");
        BOOST_REQUIRE_EQUAL(parts.size(), 3u);
        BOOST_CHECK_EQUAL(parts[2], "");
    }
    {
        auto parts = str::split(",a", ",");
        BOOST_REQUIRE_EQUAL(parts.size(), 2u);
        BOOST_CHECK_EQUAL(parts[0], "");
        BOOST_CHECK_EQUAL(parts[1], "a");
    }

    // no delimiter present, and the empty string, both give a single field
    {
        auto parts = str::split("abc", ",");
        BOOST_REQUIRE_EQUAL(parts.size(), 1u);
        BOOST_CHECK_EQUAL(parts[0], "abc");
    }
    {
        auto parts = str::split("", ",");
        BOOST_REQUIRE_EQUAL(parts.size(), 1u);
        BOOST_CHECK_EQUAL(parts[0], "");
    }

    // multi-character delimiter
    {
        auto parts = str::split("a::b::c", "::");
        BOOST_REQUIRE_EQUAL(parts.size(), 3u);
        BOOST_CHECK_EQUAL(parts[1], "b");
    }

    // rvalue string overload yields owning strings
    {
        std::vector<std::string> parts = str::split(std::string("a,b"), ",");
        BOOST_REQUIRE_EQUAL(parts.size(), 2u);
        BOOST_CHECK_EQUAL(parts[0], "a");
        BOOST_CHECK_EQUAL(parts[1], "b");
    }
}

BOOST_AUTO_TEST_CASE(test_join) {
    // a braced list is ambiguous between the string and string_view overloads, so the argument
    // type has to be named
    BOOST_CHECK_EQUAL(str::join(std::vector<std::string>{"a", "b", "c"}, "-"), "a-b-c");
    BOOST_CHECK_EQUAL(str::join(std::vector<std::string>{"a"}, "-"), "a");
    BOOST_CHECK_EQUAL(str::join(std::vector<std::string>{"a", "b"}, ""), "ab");
    BOOST_CHECK_EQUAL(str::join(std::vector<std::string>{"", ""}, ","), ",");

    // an empty list joins to nothing
    {
        std::vector<std::string> empty;
        BOOST_CHECK_EQUAL(str::join(empty, "-"), "");
    }

    // from a container
    {
        std::vector<std::string> v = {"x", "y", "z"};
        BOOST_CHECK_EQUAL(str::join(v, ", "), "x, y, z");
    }

    // the string_view overload
    {
        std::vector<std::string_view> v = {"x", "y"};
        BOOST_CHECK_EQUAL(str::join(v, "/"), "x/y");
    }

    // split and join are inverses when no field contains the delimiter
    {
        const std::string original = "a,b,c";
        auto parts = str::split(original, ",");
        BOOST_CHECK_EQUAL(str::join(parts, ","), original);
    }
}

BOOST_AUTO_TEST_CASE(test_case_conversion) {
    BOOST_CHECK_EQUAL(str::to_upper(std::string("hello")), "HELLO");
    BOOST_CHECK_EQUAL(str::to_upper(std::string("Hello World 123")), "HELLO WORLD 123");
    BOOST_CHECK_EQUAL(str::to_upper(std::string("")), "");

    BOOST_CHECK_EQUAL(str::to_lower(std::string("HELLO")), "hello");
    BOOST_CHECK_EQUAL(str::to_lower(std::string("Hello World 123")), "hello world 123");
    BOOST_CHECK_EQUAL(str::to_lower(std::string("")), "");

    // wide overloads
    BOOST_CHECK(str::to_upper(std::wstring(L"hello")) == L"HELLO");
    BOOST_CHECK(str::to_lower(std::wstring(L"HELLO")) == L"hello");

    // also reachable unqualified from namespace stdc
    BOOST_CHECK_EQUAL(to_upper(std::string("abc")), "ABC");
    BOOST_CHECK_EQUAL(to_lower(std::string("ABC")), "abc");
}

BOOST_AUTO_TEST_CASE(test_starts_ends_with) {
    BOOST_CHECK(str::starts_with("hello world", "hello"));
    BOOST_CHECK(!str::starts_with("hello world", "world"));
    BOOST_CHECK(str::starts_with("hello", "hello")); // whole string
    BOOST_CHECK(str::starts_with("hello", ""));      // empty prefix
    BOOST_CHECK(!str::starts_with("ab", "abc"));     // prefix longer than string
    BOOST_CHECK(!str::starts_with("", "a"));

    BOOST_CHECK(str::ends_with("hello world", "world"));
    BOOST_CHECK(!str::ends_with("hello world", "hello"));
    BOOST_CHECK(str::ends_with("hello", "hello"));
    BOOST_CHECK(str::ends_with("hello", ""));
    BOOST_CHECK(!str::ends_with("ab", "xab"));
    BOOST_CHECK(!str::ends_with("", "a"));

    // char overloads
    BOOST_CHECK(str::starts_with("abc", 'a'));
    BOOST_CHECK(!str::starts_with("abc", 'c'));
    BOOST_CHECK(!str::starts_with("", 'a'));
    BOOST_CHECK(str::ends_with("abc", 'c'));
    BOOST_CHECK(!str::ends_with("abc", 'a'));
    BOOST_CHECK(!str::ends_with("", 'c'));

    // wide overloads
    BOOST_CHECK(str::starts_with(std::wstring_view(L"abc"), std::wstring_view(L"ab")));
    BOOST_CHECK(str::ends_with(std::wstring_view(L"abc"), std::wstring_view(L"bc")));
    BOOST_CHECK(str::starts_with(std::wstring_view(L"abc"), L'a'));
    BOOST_CHECK(str::ends_with(std::wstring_view(L"abc"), L'c'));
}

BOOST_AUTO_TEST_CASE(test_drop) {
    using namespace std::string_view_literals;

    BOOST_CHECK_EQUAL(str::drop_front("hello"sv), "ello");
    BOOST_CHECK_EQUAL(str::drop_front("hello"sv, 3), "lo");
    BOOST_CHECK_EQUAL(str::drop_front("hello"sv, 5), "");
    BOOST_CHECK_EQUAL(str::drop_front("hello"sv, 0), "hello");

    BOOST_CHECK_EQUAL(str::drop_back("hello"sv), "hell");
    BOOST_CHECK_EQUAL(str::drop_back("hello"sv, 3), "he");
    BOOST_CHECK_EQUAL(str::drop_back("hello"sv, 5), "");
    BOOST_CHECK_EQUAL(str::drop_back("hello"sv, 0), "hello");

    // rvalue string overloads return owning strings
    BOOST_CHECK_EQUAL(str::drop_front(std::string("hello"), 2), "llo");
    BOOST_CHECK_EQUAL(str::drop_back(std::string("hello"), 2), "hel");
}

BOOST_AUTO_TEST_CASE(test_trim) {
    // A bare string literal is ambiguous between the string_view and the string&& overloads,
    // so every case below names which one it means.
    using namespace std::string_view_literals;

    // default character set is whitespace
    BOOST_CHECK_EQUAL(str::trim("  hello  "sv), "hello");
    BOOST_CHECK_EQUAL(str::ltrim("  hello  "sv), "hello  ");
    BOOST_CHECK_EQUAL(str::rtrim("  hello  "sv), "  hello");

    BOOST_CHECK_EQUAL(str::trim("\t\n hello \r\n"sv), "hello");
    BOOST_CHECK_EQUAL(str::trim("hello"sv), "hello"); // nothing to trim
    BOOST_CHECK_EQUAL(str::trim(""sv), "");
    BOOST_CHECK_EQUAL(str::trim("   "sv), ""); // all whitespace
    BOOST_CHECK_EQUAL(str::ltrim("   "sv), "");
    BOOST_CHECK_EQUAL(str::rtrim("   "sv), "");

    // inner whitespace is untouched
    BOOST_CHECK_EQUAL(str::trim("  a b  "sv), "a b");

    // single character
    BOOST_CHECK_EQUAL(str::trim("xxhelloxx"sv, 'x'), "hello");
    BOOST_CHECK_EQUAL(str::ltrim("xxhelloxx"sv, 'x'), "helloxx");
    BOOST_CHECK_EQUAL(str::rtrim("xxhelloxx"sv, 'x'), "xxhello");
    BOOST_CHECK_EQUAL(str::trim("xxxx"sv, 'x'), "");

    // explicit character set
    BOOST_CHECK_EQUAL(str::trim("[hello]"sv, "[]"), "hello");

    // rvalue string overloads
    BOOST_CHECK_EQUAL(str::trim(std::string("  hi  ")), "hi");
    BOOST_CHECK_EQUAL(str::ltrim(std::string("--hi"), '-'), "hi");
    BOOST_CHECK_EQUAL(str::rtrim(std::string("hi--"), '-'), "hi");
    BOOST_CHECK_EQUAL(str::trim(std::string("[hi]"), "[]"), "hi");

    // also reachable unqualified from namespace stdc
    BOOST_CHECK_EQUAL(trim("  hi  "sv), "hi");
}

BOOST_AUTO_TEST_CASE(test_contains) {
    BOOST_CHECK(str::contains("hello world", "lo w"));
    BOOST_CHECK(str::contains("hello", "hello"));
    BOOST_CHECK(str::contains("hello", "")); // the empty string is everywhere
    BOOST_CHECK(!str::contains("hello", "xyz"));
    BOOST_CHECK(!str::contains("", "a"));

    BOOST_CHECK(str::contains("hello", 'e'));
    BOOST_CHECK(!str::contains("hello", 'z'));
    BOOST_CHECK(!str::contains("", 'a'));
}

BOOST_AUTO_TEST_CASE(test_to_string) {
    BOOST_CHECK_EQUAL(str::to_string(true), "true");
    BOOST_CHECK_EQUAL(str::to_string(false), "false");

    BOOST_CHECK_EQUAL(str::to_string(42), "42");
    BOOST_CHECK_EQUAL(str::to_string(-1), "-1");
    BOOST_CHECK_EQUAL(str::to_string(0), "0");
    BOOST_CHECK_EQUAL(str::to_string(size_t(123)), "123");

    // floating point prints without a trailing dot
    BOOST_CHECK_EQUAL(str::to_string(3.5), "3.5");
    BOOST_CHECK_EQUAL(str::to_string(1.0), "1");
    BOOST_CHECK_EQUAL(str::to_string(0.5f), "0.5");

    BOOST_CHECK_EQUAL(str::to_string("hello"), "hello");
    BOOST_CHECK_EQUAL(str::to_string(std::string("hello")), "hello");
    BOOST_CHECK_EQUAL(str::to_string(std::string_view("hello")), "hello");

    // a single char becomes a one-character string, not an integer
    BOOST_CHECK_EQUAL(str::to_string('x'), "x");
    BOOST_CHECK_EQUAL(str::to_string('0'), "0");
    BOOST_CHECK_EQUAL(str::to_string(char(0)).size(), 1u);

    // wide input is converted to UTF-8
    BOOST_CHECK_EQUAL(str::to_string(L"wide"), "wide");
    BOOST_CHECK_EQUAL(str::to_string(std::wstring(L"wide")), "wide");
    BOOST_CHECK_EQUAL(str::to_string(std::wstring_view(L"wide")), "wide");
    BOOST_CHECK_EQUAL(str::to_string(L'x'), "x");

    // paths come out with native separators
    {
        auto actual = str::to_string(fs::path("a/b"));
#ifdef _WIN32
        BOOST_CHECK_EQUAL(actual, "a\\b");
#else
        BOOST_CHECK_EQUAL(actual, "a/b");
#endif
    }
}

BOOST_AUTO_TEST_CASE(test_format) {
    {
        std::string actual = formatN("%1 %2 %3 %2 %1", "alice", "bob", "cindy");
        std::string expect = "alice bob cindy bob alice";
        BOOST_CHECK(actual == expect);
    }

    {
        std::string actual = formatN("%% %1 %5 %2 %X %", "foo", "bar");
        std::string expect = "% foo %5 bar %X %";
        BOOST_CHECK(actual == expect);
    }

    {
        std::string actual = formatN("%10 %12", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
        std::string expect = "10 12";
        BOOST_CHECK(actual == expect);
    }

    // no placeholders, no arguments
    BOOST_CHECK_EQUAL(formatN("plain"), "plain");
    BOOST_CHECK_EQUAL(formatN(""), "");
    BOOST_CHECK_EQUAL(formatN("", "unused"), "");

    // adjacent and repeated placeholders
    BOOST_CHECK_EQUAL(formatN("%1%2", "a", "b"), "ab");
    BOOST_CHECK_EQUAL(formatN("%1%1%1", "x"), "xxx");
    BOOST_CHECK_EQUAL(formatN("[%1]", "x"), "[x]");

    // out-of-range and malformed indices are left as written
    BOOST_CHECK_EQUAL(formatN("%9", "a"), "%9");
    BOOST_CHECK_EQUAL(formatN("%0", "a"), "%0"); // indices start at 1
    BOOST_CHECK_EQUAL(formatN("%", "a"), "%");
    BOOST_CHECK_EQUAL(formatN("%a", "x"), "%a");

    // %% is an escaped percent
    BOOST_CHECK_EQUAL(formatN("100%%"), "100%%"); // no args: returned verbatim
    BOOST_CHECK_EQUAL(formatN("100%% %1", "done"), "100% done");

    // mixed argument types are converted through to_string()
    BOOST_CHECK_EQUAL(formatN("%1 %2 %3 %4", 1, true, 2.5, "s"), "1 true 2.5 s");
    BOOST_CHECK_EQUAL(formatN("%1%2%3", 'a', 'b', 'c'), "abc");
    BOOST_CHECK_EQUAL(formatN("%1 %2", 'x', L'y'), "x y");

    // an empty argument substitutes nothing
    BOOST_CHECK_EQUAL(formatN("[%1]", ""), "[]");

    // the underlying format() takes the arguments as a list
    BOOST_CHECK_EQUAL(str::format("%1-%2", {"a", "b"}), "a-b");
    BOOST_CHECK_EQUAL(str::format("%1", {}), "%1");
}

BOOST_AUTO_TEST_CASE(test_varexp) {
    const std::map<std::string, std::string> vars{
        {"FOO",   "Hello" },
        {"BAR",   "World" },
        {"EMPTY", ""      },
        {"A",     "X"     },
        {"B",     "Y"     },
        {"X_Y",   "nested"},
    };

    BOOST_CHECK_EQUAL(str::varexp("${FOO} ${BAR}!", vars), "Hello World!");
    BOOST_CHECK_EQUAL(str::varexp("${FOO}", vars), "Hello");
    BOOST_CHECK_EQUAL(str::varexp("a${FOO}b", vars), "aHellob");

    // nested expansion: the inner names are resolved first
    BOOST_CHECK_EQUAL(str::varexp("${${A}_${B}} World!", vars), "nested World!");

    // an unknown name expands to nothing, as does a name bound to the empty string
    BOOST_CHECK_EQUAL(str::varexp("[${NOPE}]", vars), "[]");
    BOOST_CHECK_EQUAL(str::varexp("[${EMPTY}]", vars), "[]");

    // text without variables passes through untouched
    BOOST_CHECK_EQUAL(str::varexp("no variables here", vars), "no variables here");
    BOOST_CHECK_EQUAL(str::varexp("", vars), "");
    BOOST_CHECK_EQUAL(str::varexp("100$", vars), "100$");
    BOOST_CHECK_EQUAL(str::varexp("a$b", vars), "a$b");

    // an unbalanced brace is rejected: the result is empty
    BOOST_CHECK_EQUAL(str::varexp("${FOO", vars), "");
    BOOST_CHECK_EQUAL(str::varexp("a ${ b", vars), "");

    // the callback form
    {
        auto find = [](const std::string_view &name) -> std::string {
            return std::string(name) + "!";
        };
        BOOST_CHECK_EQUAL(str::varexp("${a} ${b}", find), "a! b!");
    }
}

BOOST_AUTO_TEST_CASE(test_codec_convert) {
    {
        std::wstring actual = wstring_conv::from_utf8("HelloWorld");
        std::wstring expect = L"HelloWorld";
        BOOST_CHECK(actual == expect);
    }

    {
        std::string actual = wstring_conv::to_utf8(L"HelloWorld");
        std::string expect = "HelloWorld";
        BOOST_CHECK(actual == expect);
    }

    // the empty string round trips
    BOOST_CHECK(wstring_conv::from_utf8("").empty());
    BOOST_CHECK(wstring_conv::to_utf8(L"").empty());

    // explicit lengths stop early instead of running to the terminator
    BOOST_CHECK(wstring_conv::from_utf8("abcdef", 3) == L"abc");
    BOOST_CHECK(wstring_conv::to_utf8(L"abcdef", 3) == "abc");

    // non-ASCII round trip ("中文测试", spelled out in bytes so the source encoding
    // cannot affect the test)
    {
        const std::string utf8 = "\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95";
        auto wide = wstring_conv::from_utf8(utf8);
        BOOST_CHECK_EQUAL(wide.size(), 4u); // four code units on both UTF-16 and UTF-32
        BOOST_CHECK_EQUAL(wstring_conv::to_utf8(wide), utf8);
    }

#ifdef _WIN32
    {
        std::wstring actual = wstring_conv::from_ansi("HelloWorld");
        std::wstring expect = L"HelloWorld";
        BOOST_CHECK(actual == expect);
    }

    {
        std::string actual = wstring_conv::to_ansi(L"HelloWorld");
        std::string expect = "HelloWorld";
        BOOST_CHECK(actual == expect);
    }
#endif
}

namespace {

    // vasprintf() can only be reached through a variadic function.
    std::string call_vasprintf(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        std::string res = str::vasprintf(fmt, args);
        va_end(args);
        return res;
    }

}

BOOST_AUTO_TEST_CASE(test_asprintf) {
    {
        std::string actual = asprintf("a=%d, b=%s, c=%p", 123, "hello", printf);
        std::string expect;
        expect.resize(100);
        size_t size = snprintf(&expect[0], expect.size(), "a=%d, b=%s, c=%p", 123, "hello", printf);
        expect.resize(size);
        BOOST_CHECK(actual == expect);
    }

    BOOST_CHECK_EQUAL(asprintf("no args"), "no args");
    BOOST_CHECK_EQUAL(asprintf(""), "");
    BOOST_CHECK_EQUAL(asprintf("%d%%", 50), "50%");
    BOOST_CHECK_EQUAL(asprintf("%5d|", 42), "   42|");
    BOOST_CHECK_EQUAL(asprintf("%.2f", 3.14159), "3.14");

    // a result well past any small internal buffer
    {
        std::string long_arg(4096, 'x');
        std::string actual = asprintf("[%s]", long_arg.c_str());
        BOOST_CHECK_EQUAL(actual.size(), long_arg.size() + 2);
        BOOST_CHECK_EQUAL(actual.front(), '[');
        BOOST_CHECK_EQUAL(actual.back(), ']');
        BOOST_CHECK_EQUAL(actual.substr(1, long_arg.size()), long_arg);
    }

    // vasprintf() is the same formatting, taking an assembled va_list
    BOOST_CHECK_EQUAL(call_vasprintf("a=%d, b=%s", 7, "x"), "a=7, b=x");
    BOOST_CHECK_EQUAL(call_vasprintf("plain"), "plain");
}

BOOST_AUTO_TEST_SUITE_END()
