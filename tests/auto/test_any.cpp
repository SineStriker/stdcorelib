// SPDX-License-Identifier: MIT

#include <memory>
#include <string>
#include <vector>

#include <stdcorelib/adt/any.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_any)

namespace {

    struct Tracked {
        static int alive;
        int value;

        explicit Tracked(int v = 0) : value(v) {
            ++alive;
        }
        Tracked(const Tracked &other) : value(other.value) {
            ++alive;
        }
        Tracked &operator=(const Tracked &) = default;
        ~Tracked() {
            --alive;
        }
    };

    int Tracked::alive = 0;

    struct Base {
        int b = 1;
    };
    struct Derived : Base {
        int d = 2;
    };

}

BOOST_AUTO_TEST_CASE(test_empty) {
    any value;
    BOOST_CHECK(!value.has_value());
    BOOST_CHECK(value.type_name().empty());
    BOOST_CHECK(any_cast<int>(&value) == nullptr);
    BOOST_CHECK(!value.holds<int>());
}

BOOST_AUTO_TEST_CASE(test_round_trip) {
    any value = 42;
    BOOST_REQUIRE(value.has_value());
    BOOST_CHECK(value.holds<int>());
    BOOST_REQUIRE(any_cast<int>(&value) != nullptr);
    BOOST_CHECK_EQUAL(*any_cast<int>(&value), 42);

    // a type that is not trivially copyable, so the storage has real work to do
    any text = std::string("hello");
    BOOST_REQUIRE(any_cast<std::string>(&text) != nullptr);
    BOOST_CHECK_EQUAL(*any_cast<std::string>(&text), "hello");

    // and one that is only movable into place
    any vec = std::vector<int>{1, 2, 3};
    BOOST_REQUIRE(any_cast<std::vector<int>>(&vec) != nullptr);
    BOOST_CHECK_EQUAL(any_cast<std::vector<int>>(&vec)->size(), 3u);
}

BOOST_AUTO_TEST_CASE(test_wrong_type_is_null_not_garbage) {
    any value = 42;
    BOOST_CHECK(any_cast<long>(&value) == nullptr); // a different type of the same size
    BOOST_CHECK(any_cast<unsigned>(&value) == nullptr);
    BOOST_CHECK(any_cast<std::string>(&value) == nullptr);
    BOOST_CHECK(!value.holds<long>());
}

// Only the exact type comes back. This is the same rule std::any follows, and it is worth a test
// because reading a Derived as a Base looks reasonable until it silently does not work.
BOOST_AUTO_TEST_CASE(test_no_conversion_to_base) {
    any value = Derived{};
    BOOST_CHECK(value.holds<Derived>());
    BOOST_CHECK(!value.holds<Base>());
    BOOST_CHECK(any_cast<Base>(&value) == nullptr);
}

// const and reference qualifiers are stripped on the way in, so they cannot be asked for on the
// way out either.
BOOST_AUTO_TEST_CASE(test_qualifiers_are_stripped) {
    const int original = 7;
    const int &ref = original;
    any value = ref;

    BOOST_CHECK(value.holds<int>());
    BOOST_CHECK(value.holds<const int>());
    BOOST_CHECK(value.holds<const int &>());
    BOOST_REQUIRE(any_cast<int>(&value) != nullptr);
    BOOST_CHECK_EQUAL(*any_cast<int>(&value), 7);

    // a string literal decays to a pointer, which is the type that comes back out
    any literal = "text";
    BOOST_CHECK(literal.holds<const char *>());
}

BOOST_AUTO_TEST_CASE(test_copy_and_move) {
    BOOST_REQUIRE_EQUAL(Tracked::alive, 0);
    {
        any value = Tracked{5};
        BOOST_CHECK_EQUAL(Tracked::alive, 1);

        any copy = value;
        BOOST_CHECK_EQUAL(Tracked::alive, 2); // a copy really is a copy
        BOOST_CHECK_EQUAL(any_cast<Tracked>(&copy)->value, 5);
        any_cast<Tracked>(&copy)->value = 6;
        BOOST_CHECK_EQUAL(any_cast<Tracked>(&value)->value, 5); // and is independent

        any moved = std::move(copy);
        BOOST_CHECK_EQUAL(Tracked::alive, 2); // moving does not
        BOOST_CHECK(!copy.has_value());
        BOOST_CHECK_EQUAL(any_cast<Tracked>(&moved)->value, 6);
    }
    BOOST_CHECK_EQUAL(Tracked::alive, 0); // and everything is destroyed
}

BOOST_AUTO_TEST_CASE(test_assignment_and_reset) {
    any value = 1;
    value = std::string("now a string");
    BOOST_CHECK(value.holds<std::string>());
    BOOST_CHECK(!value.holds<int>());

    value.reset();
    BOOST_CHECK(!value.has_value());

    // self assignment through the by value parameter has to leave the value alone
    any keep = 99;
    keep = keep;
    BOOST_REQUIRE(keep.has_value());
    BOOST_CHECK_EQUAL(*any_cast<int>(&keep), 99);
}

BOOST_AUTO_TEST_CASE(test_swap) {
    any left = 1;
    any right = std::string("right");
    swap(left, right);
    BOOST_CHECK(left.holds<std::string>());
    BOOST_CHECK(right.holds<int>());
}

BOOST_AUTO_TEST_CASE(test_type_name_is_readable) {
    any value = 42;
    // the exact spelling is the compiler's, so only look for the part every one of them agrees on
    BOOST_CHECK(value.type_name().find("int") != std::string_view::npos);

    any text = std::string();
    BOOST_CHECK(text.type_name().find("string") != std::string_view::npos);
}

// The identity of a type is a name looked up in a table, so two entries that were never compared
// before still have to agree, and a type never seen before has to get an answer of its own.
BOOST_AUTO_TEST_CASE(test_identity_is_stable) {
    any first = 42;
    any second = 43;
    BOOST_CHECK(first.holds<int>() && second.holds<int>());
    BOOST_CHECK_EQUAL(first.type_name(), second.type_name());

    struct NeverSeenBefore {
        int x;
    };
    any fresh = NeverSeenBefore{1};
    BOOST_CHECK(fresh.holds<NeverSeenBefore>());
    BOOST_CHECK(!fresh.holds<int>());
}

#ifdef STDCORELIB_EXCEPTIONS
BOOST_AUTO_TEST_CASE(test_value_cast_throws_on_the_wrong_type) {
    any value = 42;
    BOOST_CHECK_EQUAL(any_cast<int>(value), 42);
    BOOST_CHECK_THROW(any_cast<std::string>(value), bad_any_cast);

    const any &constant = value;
    BOOST_CHECK_EQUAL(any_cast<int>(constant), 42);
    BOOST_CHECK_THROW(any_cast<std::string>(constant), bad_any_cast);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
