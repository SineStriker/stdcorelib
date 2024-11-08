#include <stdcorelib/format.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_format)

BOOST_AUTO_TEST_CASE(test_format_text) {
    {
        std::string actual = formatTextN("%1 %2 %3 %2 %1", "alice", "bob", "cindy");
        std::string expect = "alice bob cindy bob alice";
        BOOST_CHECK(actual == expect);
    }

    {
        std::string actual = formatTextN("%% %1 %5 %2 %X %", "foo", "bar");
        std::string expect = "% foo %5 bar %X %";
        BOOST_CHECK(actual == expect);
    }

    {
        std::string actual = formatTextN("%10 %12", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
        std::string expect = "10 12";
        BOOST_CHECK(actual == expect);
    }
}

BOOST_AUTO_TEST_SUITE_END()