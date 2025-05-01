#include <vector>

#include <stdcorelib/linked_map.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_linked_map)

BOOST_AUTO_TEST_CASE(test_insert_erase) {
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
    auto check = [&]() {
        int i = 0;
        for (const auto &item : map) {
            const auto &expect = kvs[i++];
            const auto &actual = item;
            BOOST_CHECK(expect.first == actual.first);
            BOOST_CHECK(expect.second == actual.second);
        }
    };

    // Reverse check
    auto reverse_check = [&]() {
        int i = kvs.size();
        for (auto it = map.rbegin(); it != map.rend(); ++it) {
            const auto &expect = kvs[--i];
            const auto &actual = *it;
            BOOST_CHECK(expect.first == actual.first);
            BOOST_CHECK(expect.second == actual.second);
        }
    };

    check();
    reverse_check();

    // Erase
    {
        map.erase("2");
        BOOST_CHECK(map.size() == 2);
        BOOST_CHECK(map.find("2") == map.end());

        kvs.erase(kvs.begin() + 1);
    }

    check();
    reverse_check();

    // Duplicated insert (should fail)
    {
        auto it = map.append("1", 3);
        BOOST_CHECK(it.second == false);
        BOOST_CHECK(it.first == map.find("1"));
        BOOST_CHECK(map.size() == 2);
        BOOST_CHECK(map.find("1")->second == 1);
    }

    check();
    reverse_check();

    // Operator[]
    {
        map["1"] = 3;
        BOOST_CHECK(map.size() == 2);
        BOOST_CHECK(map.find("1")->second == 3);
    }
}

BOOST_AUTO_TEST_SUITE_END()