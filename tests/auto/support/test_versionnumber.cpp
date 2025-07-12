#include <stdcorelib/support/versionnumber.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_versionnumber)

BOOST_AUTO_TEST_CASE(test_fromString) {
    stdc::VersionNumber v;

    v = VersionNumber::fromString("1");
    BOOST_CHECK(v == VersionNumber(1));

    v = VersionNumber::fromString("1.2");
    BOOST_CHECK(v == VersionNumber(1, 2));

    v = VersionNumber::fromString("1.2.3");
    BOOST_CHECK(v == VersionNumber(1, 2, 3));

    v = VersionNumber::fromString("1.2.3.4");
    BOOST_CHECK(v == VersionNumber(1, 2, 3, 4));

    v = VersionNumber::fromString("1.2.3.4.5");
    BOOST_CHECK(v == VersionNumber(1, 2, 3, 4));

    // error cases
    v = VersionNumber::fromString("1.x");
    BOOST_CHECK(v == VersionNumber(1));

    v = VersionNumber::fromString("1.2.x");
    BOOST_CHECK(v == VersionNumber(1, 2));

    v = VersionNumber::fromString("xxx");
    BOOST_CHECK(v == VersionNumber());
}

BOOST_AUTO_TEST_CASE(test_compare) {
    stdc::VersionNumber v1(1, 2, 3);
    stdc::VersionNumber v2(1, 2, 3);
    stdc::VersionNumber v3(1, 2, 4);
    stdc::VersionNumber v4(1, 3, 3);
    stdc::VersionNumber v5(2, 2, 3);

    BOOST_CHECK(v1 == v2);
    BOOST_CHECK(v1 < v3);
    BOOST_CHECK(v1 < v4);
    BOOST_CHECK(v1 < v5);
    BOOST_CHECK(v3 > v1);
    BOOST_CHECK(v4 > v1);
    BOOST_CHECK(v5 > v1);
}

BOOST_AUTO_TEST_SUITE_END()