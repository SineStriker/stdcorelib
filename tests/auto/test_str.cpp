#include <stdcorelib/str.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_str)

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
}

BOOST_AUTO_TEST_CASE(test_varexp) {
    {
        std::map<std::string, std::string> vars{
            {"FOO", "Hello"}
        };
        std::string actual = str::varexp("${FOO} World!", vars);
        std::string expect = "Hello World!";
        BOOST_CHECK(actual == expect);
    }

    {
        std::map<std::string, std::string> vars{
            {"FOO", "A"    },
            {"BAR", "B"    },
            {"A_B", "Hello"}
        };
        std::string actual = str::varexp("${${FOO}_${BAR}} World!", vars);
        std::string expect = "Hello World!";
        BOOST_CHECK(actual == expect);
    }

    {
        std::string actual = formatN("%10 %12", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
        std::string expect = "10 12";
        BOOST_CHECK(actual == expect);
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

#ifdef _WIN32
    {
        std::wstring actual = wstring_conv::from_ansi("HelloWorld");
        std::wstring expect = L"HelloWorld";
        BOOST_CHECK(actual == expect);
    }
#endif
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

    {
        std::string actual = formatN("%% %1 %5 %2 %X %", "foo", "bar");
        std::string expect = "% foo %5 bar %X %";
        BOOST_CHECK(actual == expect);
    }
}

BOOST_AUTO_TEST_SUITE_END()