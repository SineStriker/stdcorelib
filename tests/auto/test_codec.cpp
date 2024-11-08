#include <stdcorelib/codec.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_codec)

BOOST_AUTO_TEST_CASE(test_codec_convert) {
    {
        std::wstring actual = utf8ToWide("HelloWorld");
        std::wstring expect = L"HelloWorld";
        BOOST_CHECK(actual == expect);
    }

    {
        std::string actual = wideToUtf8(L"HelloWorld");
        std::string expect = "HelloWorld";
        BOOST_CHECK(actual == expect);
    }

#ifdef _WIN32
    {
        std::string actual = ansiToUtf8("HelloWorld");
        std::string expect = "HelloWorld";
        BOOST_CHECK(actual == expect);
    }
#endif
}

BOOST_AUTO_TEST_SUITE_END()