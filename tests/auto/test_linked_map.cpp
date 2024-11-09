#include <vector>

#include <stdcorelib/linked_map.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_linked_map)

BOOST_AUTO_TEST_CASE(test_linked_map_append) {
    std::vector<std::pair<std::string, int>> kvs{
        {"1", 1},
        {"2", 2},
        {"3", 3},
    };

    linked_map<std::string, int> map;
    for (const auto &item : std::as_const(kvs)) {
        map[item.first] = item.second;
    }

    // Check
    {
        int i = 0;
        for (const auto &item : map) {
            const auto &expect = kvs[i++];
            const auto &actual = item;
            BOOST_CHECK(expect.first == actual.first);
            BOOST_CHECK(expect.second == actual.second);
        }
    }

    {
        int i = kvs.size();
        for (auto it = map.rbegin(); it != map.rend(); ++it) {
            const auto &expect = kvs[--i];
            const auto &actual = *it;
            BOOST_CHECK(expect.first == actual.first);
            BOOST_CHECK(expect.second == actual.second);
        }
    }

    std::unordered_map<std::string, int> map2;
    map2["1"] = 1;

    map2.emplace("1", 2);
}

BOOST_AUTO_TEST_SUITE_END()