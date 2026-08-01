#include <stdcorelib/stlextra/algorithms.h>

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_algorithms)

namespace {

    // Counts live instances so delete_all() can be checked for having freed everything.
    struct Tracked {
        Tracked() {
            g_count++;
        }
        ~Tracked() {
            g_count--;
        }

        static inline int g_count = 0;
    };

}

BOOST_AUTO_TEST_CASE(test_type_traits) {
    // key_type
    static_assert(has_key_type<std::map<int, int>>::value);
    static_assert(has_key_type<std::set<int>>::value);
    static_assert(!has_key_type<std::vector<int>>::value);
    static_assert(!has_key_type<int>::value);

    // mapped_type
    static_assert(has_mapped_type<std::map<int, int>>::value);
    static_assert(!has_mapped_type<std::set<int>>::value);
    static_assert(!has_mapped_type<std::vector<int>>::value);

    // is_map == both of the above
    static_assert(is_map<std::map<int, int>>::value);
    static_assert(is_map<std::unordered_map<int, int>>::value);
    static_assert(!is_map<std::set<int>>::value);
    static_assert(!is_map<std::vector<int>>::value);
    static_assert(!is_map<std::string>::value);

    BOOST_CHECK(true); // the checks above are compile time
}

BOOST_AUTO_TEST_CASE(test_delete_all_iterators) {
    std::vector<Tracked *> v;
    for (int i = 0; i < 4; ++i) {
        v.push_back(new Tracked());
    }
    BOOST_CHECK_EQUAL(Tracked::g_count, 4);

    delete_all(v.begin(), v.end());
    BOOST_CHECK_EQUAL(Tracked::g_count, 0);

    // an empty range is a no-op
    std::vector<Tracked *> empty;
    delete_all(empty.begin(), empty.end());
    BOOST_CHECK_EQUAL(Tracked::g_count, 0);
}

BOOST_AUTO_TEST_CASE(test_delete_all_container) {
    // sequence container: the elements themselves are the pointers
    {
        std::vector<Tracked *> v = {new Tracked(), new Tracked()};
        BOOST_CHECK_EQUAL(Tracked::g_count, 2);
        delete_all(v);
        BOOST_CHECK_EQUAL(Tracked::g_count, 0);
    }

    // map: the mapped values are the pointers, the keys are left alone
    {
        std::map<std::string, Tracked *> m;
        m["a"] = new Tracked();
        m["b"] = new Tracked();
        m["c"] = new Tracked();
        BOOST_CHECK_EQUAL(Tracked::g_count, 3);
        delete_all(m);
        BOOST_CHECK_EQUAL(Tracked::g_count, 0);
    }

    // unordered_map takes the same branch
    {
        std::unordered_map<int, Tracked *> m;
        m[1] = new Tracked();
        BOOST_CHECK_EQUAL(Tracked::g_count, 1);
        delete_all(m);
        BOOST_CHECK_EQUAL(Tracked::g_count, 0);
    }
}

BOOST_AUTO_TEST_CASE(test_contains) {
    std::set<int> s = {1, 2, 3};
    BOOST_CHECK(contains(s, 2));
    BOOST_CHECK(!contains(s, 9));

    std::map<std::string, int> m = {
        {"a", 1},
        {"b", 2},
    };
    BOOST_CHECK(contains(m, std::string("a")));
    BOOST_CHECK(!contains(m, std::string("z")));

    std::unordered_set<std::string> us = {"x"};
    BOOST_CHECK(contains(us, std::string("x")));
    BOOST_CHECK(!contains(us, std::string("y")));

    std::set<int> empty;
    BOOST_CHECK(!contains(empty, 0));
}

BOOST_AUTO_TEST_CASE(test_hash) {
    // hash() keeps the low 32 bits of the key and xors in the seed
    static_assert(hash(0) == 0);
    static_assert(hash(1) == 1);

    BOOST_CHECK_EQUAL(hash(123), size_t(123));
    BOOST_CHECK_EQUAL(hash(123, 0), size_t(123));

    // the seed is xored in, so seeding twice with the same value cancels out
    BOOST_CHECK_EQUAL(hash(0, 0xABCD), size_t(0xABCD));
    BOOST_CHECK_EQUAL(hash(123, 0) ^ 0xABCD, hash(123, 0xABCD));

    // different keys give different results (within the low 32 bits)
    BOOST_CHECK(hash(1) != hash(2));

    // only the low 32 bits of the key survive
    BOOST_CHECK_EQUAL(hash(size_t(0xFFFFFFFFull)), size_t(0xFFFFFFFFull));
    if constexpr (sizeof(size_t) > 4) {
        BOOST_CHECK_EQUAL(hash(size_t(1) << 32), size_t(0));
    }
}

BOOST_AUTO_TEST_SUITE_END()
