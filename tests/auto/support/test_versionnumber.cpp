#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <stdcorelib/support/versionnumber.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_versionnumber)

BOOST_AUTO_TEST_CASE(test_construct) {
    // default is all zeros
    {
        VersionNumber v;
        BOOST_CHECK_EQUAL(v.major(), 0);
        BOOST_CHECK_EQUAL(v.minor(), 0);
        BOOST_CHECK_EQUAL(v.patch(), 0);
        BOOST_CHECK_EQUAL(v.tweak(), 0);
        BOOST_CHECK(v.isEmpty());
    }

    // the trailing components default to zero
    {
        VersionNumber v(1);
        BOOST_CHECK_EQUAL(v.major(), 1);
        BOOST_CHECK_EQUAL(v.minor(), 0);
        BOOST_CHECK_EQUAL(v.patch(), 0);
        BOOST_CHECK_EQUAL(v.tweak(), 0);
        BOOST_CHECK(!v.isEmpty());
    }

    {
        VersionNumber v(1, 2, 3, 4);
        BOOST_CHECK_EQUAL(v.major(), 1);
        BOOST_CHECK_EQUAL(v.minor(), 2);
        BOOST_CHECK_EQUAL(v.patch(), 3);
        BOOST_CHECK_EQUAL(v.tweak(), 4);
    }

    // any non-zero component makes it non-empty
    BOOST_CHECK(!VersionNumber(0, 0, 0, 1).isEmpty());
    BOOST_CHECK(!VersionNumber(0, 1).isEmpty());
    BOOST_CHECK(VersionNumber(0, 0, 0, 0).isEmpty());
}

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

    // the empty string parses to the empty version
    BOOST_CHECK(VersionNumber::fromString("") == VersionNumber());

    // an unparsable component is left at zero, later ones still parse
    BOOST_CHECK(VersionNumber::fromString("x.2") == VersionNumber(0, 2));

    // leading zeros are accepted
    BOOST_CHECK(VersionNumber::fromString("01.02.03") == VersionNumber(1, 2, 3));

    // large components
    BOOST_CHECK(VersionNumber::fromString("2024.11.30") == VersionNumber(2024, 11, 30));
}

BOOST_AUTO_TEST_CASE(test_toString) {
    // trailing zero components are dropped, but never below "major.minor"
    BOOST_CHECK_EQUAL(VersionNumber(1, 2, 3, 4).toString(), "1.2.3.4");
    BOOST_CHECK_EQUAL(VersionNumber(1, 2, 3).toString(), "1.2.3");
    BOOST_CHECK_EQUAL(VersionNumber(1, 2).toString(), "1.2");
    BOOST_CHECK_EQUAL(VersionNumber(1).toString(), "1.0");
    BOOST_CHECK_EQUAL(VersionNumber().toString(), "0.0");

    // a non-zero tweak keeps the zero components before it
    BOOST_CHECK_EQUAL(VersionNumber(1, 0, 0, 4).toString(), "1.0.0.4");
    BOOST_CHECK_EQUAL(VersionNumber(1, 0, 3).toString(), "1.0.3");

    // round trip through fromString for the forms toString can produce
    for (const auto &v : {VersionNumber(1, 2, 3, 4), VersionNumber(1, 2, 3), VersionNumber(1, 2),
                          VersionNumber(1), VersionNumber()}) {
        BOOST_CHECK(VersionNumber::fromString(v.toString()) == v);
    }
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

    // inequality
    BOOST_CHECK(!(v1 != v2));
    BOOST_CHECK(v1 != v3);

    // the tweak component takes part in ordering
    BOOST_CHECK(VersionNumber(1, 2, 3, 1) > VersionNumber(1, 2, 3));
    BOOST_CHECK(VersionNumber(1, 2, 3) < VersionNumber(1, 2, 3, 1));
    BOOST_CHECK(VersionNumber(1, 2, 3, 4) != VersionNumber(1, 2, 3, 5));

    // <= and >= are the non-strict forms
    BOOST_CHECK(v1 <= v2);
    BOOST_CHECK(v1 >= v2);
    BOOST_CHECK(v1 <= v3);
    BOOST_CHECK(v3 >= v1);
    BOOST_CHECK(!(v3 <= v1));
    BOOST_CHECK(!(v1 >= v3));

    // a strict ordering: nothing is less than itself
    BOOST_CHECK(!(v1 < v1));
    BOOST_CHECK(!(v1 > v1));

    // an earlier component dominates the later ones
    BOOST_CHECK(VersionNumber(2, 0, 0, 0) > VersionNumber(1, 99, 99, 99));
    BOOST_CHECK(VersionNumber(1, 2, 0, 0) > VersionNumber(1, 1, 99, 99));
}

BOOST_AUTO_TEST_CASE(test_hash) {
    std::hash<VersionNumber> hasher;

    // equal versions hash equally
    BOOST_CHECK_EQUAL(hasher(VersionNumber(1, 2, 3)), hasher(VersionNumber(1, 2, 3)));
    BOOST_CHECK_EQUAL(hasher(VersionNumber()), hasher(VersionNumber()));

    // different versions normally hash differently
    BOOST_CHECK(hasher(VersionNumber(1, 2, 3)) != hasher(VersionNumber(1, 2, 4)));
    BOOST_CHECK(hasher(VersionNumber(1, 0)) != hasher(VersionNumber(2, 0)));

    // ...but the components are folded together with xor, which is commutative, so any
    // permutation of them collides. Legal for a hash, just weak: keep it in mind before
    // using VersionNumber as a key in a container that has to stay fast.
    BOOST_CHECK_EQUAL(hasher(VersionNumber(1, 2, 3)), hasher(VersionNumber(3, 2, 1)));
    BOOST_CHECK_EQUAL(hasher(VersionNumber(1, 2)), hasher(VersionNumber(2, 1)));

    // usable as a key in the unordered containers
    std::unordered_set<VersionNumber> set;
    set.insert(VersionNumber(1, 0));
    set.insert(VersionNumber(1, 0)); // duplicate
    set.insert(VersionNumber(2, 0));
    BOOST_CHECK_EQUAL(set.size(), 2u);
    BOOST_CHECK(set.count(VersionNumber(1, 0)) == 1);
    BOOST_CHECK(set.count(VersionNumber(3, 0)) == 0);

    std::unordered_map<VersionNumber, std::string> map;
    map[VersionNumber(1, 2, 3)] = "a";
    map[VersionNumber(1, 2, 3)] = "b"; // overwrites
    BOOST_CHECK_EQUAL(map.size(), 1u);
    BOOST_CHECK_EQUAL(map[VersionNumber(1, 2, 3)], "b");
}

BOOST_AUTO_TEST_CASE(test_ostream) {
    std::ostringstream oss;
    oss << VersionNumber(1, 2, 3);
    BOOST_CHECK_EQUAL(oss.str(), "VersionNumber(1.2.3)");

    std::ostringstream oss2;
    oss2 << VersionNumber();
    BOOST_CHECK_EQUAL(oss2.str(), "VersionNumber(0.0)");
}

BOOST_AUTO_TEST_SUITE_END()
