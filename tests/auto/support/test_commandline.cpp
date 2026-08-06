// SPDX-License-Identifier: MIT

#include <cstdint>
#include <string>

#include <stdcorelib/support/commandline.h>

#include <boost/test/unit_test.hpp>

using namespace stdc::cli;

namespace {

    /// Reads \a token as a \c T, saying whether it could be read at all.
    template <class T>
    bool reads(std::string_view token, T *out) {
        return value_traits<T>::parse(token, out);
    }

    /// Reads \a token as a \c T that is expected to succeed, for the cases where only the value
    /// is interesting.
    template <class T>
    T read(std::string_view token) {
        T out{};
        BOOST_REQUIRE_MESSAGE(reads(token, &out), "could not read \"" << token << "\"");
        return out;
    }

    struct Fraction {
        int numerator = 0;
        int denominator = 1;
    };

}

/// A type of the caller's own, to check that the customization point is reachable from outside
/// the library and that a type carrying its own syntax works.
template <>
struct stdc::cli::value_traits<Fraction> {
    static bool parse(std::string_view token, Fraction *out) {
        auto slash = token.find('/');
        if (slash == std::string_view::npos) {
            return false;
        }
        return value_traits<int>::parse(token.substr(0, slash), &out->numerator) &&
               value_traits<int>::parse(token.substr(slash + 1), &out->denominator);
    }
    static const char *type_name() {
        return "fraction";
    }
};

BOOST_AUTO_TEST_SUITE(test_commandline)

BOOST_AUTO_TEST_CASE(test_string_takes_anything) {
    BOOST_CHECK_EQUAL(read<std::string>(""), "");
    BOOST_CHECK_EQUAL(read<std::string>("--not-an-option"), "--not-an-option");
    BOOST_CHECK_EQUAL(read<std::string>(" spaces kept "), " spaces kept ");

    // The view alternative sees the same bytes rather than a copy.
    std::string_view token = "borrowed";
    std::string_view view;
    BOOST_REQUIRE(reads(token, &view));
    BOOST_CHECK(view.data() == token.data());
}

BOOST_AUTO_TEST_CASE(test_integers) {
    BOOST_CHECK_EQUAL(read<int>("0"), 0);
    BOOST_CHECK_EQUAL(read<int>("42"), 42);
    BOOST_CHECK_EQUAL(read<int>("-42"), -42);
    BOOST_CHECK_EQUAL(read<int>("+42"), 42);

    int out;
    // A number has to be the whole token. Half of one is not a number.
    BOOST_CHECK(!reads("12abc", &out));
    BOOST_CHECK(!reads("", &out));
    BOOST_CHECK(!reads(" 12", &out));
    BOOST_CHECK(!reads("12 ", &out));
    BOOST_CHECK(!reads("1.5", &out));
    BOOST_CHECK(!reads("0x10", &out));
    BOOST_CHECK(!reads("--", &out));
}

BOOST_AUTO_TEST_CASE(test_integer_range_belongs_to_the_target_type) {
    // The check is the range of the type asked for, not of int64_t, so a value that fits nothing
    // narrower is refused where a narrower type was wanted.
    BOOST_CHECK_EQUAL(read<uint8_t>("255"), 255);
    uint8_t small;
    BOOST_CHECK(!reads("256", &small));

    BOOST_CHECK_EQUAL(read<int8_t>("-128"), -128);
    int8_t signed_small;
    BOOST_CHECK(!reads("-129", &signed_small));

    BOOST_CHECK_EQUAL(read<int64_t>("9223372036854775807"), INT64_MAX);
    int64_t big;
    BOOST_CHECK(!reads("9223372036854775808", &big));

    BOOST_CHECK_EQUAL(read<uint64_t>("18446744073709551615"), UINT64_MAX);
}

// This one pins a promise of the standard library rather than of the code above it: from_chars
// into an unsigned rejects a minus by itself, on all three of MSVC, libstdc++ and libc++, so
// nothing here refuses one by hand. If that ever stops being true, this is where it shows.
BOOST_AUTO_TEST_CASE(test_negative_is_not_an_unsigned) {
    unsigned out;
    BOOST_CHECK(!reads("-1", &out));
    BOOST_CHECK(!reads("-0", &out));
    BOOST_CHECK(!reads("-", &out));

    // A plus is refused by from_chars too, and is dropped before it gets there.
    BOOST_CHECK_EQUAL(read<unsigned>("+7"), 7u);
}

BOOST_AUTO_TEST_CASE(test_floating_point) {
    BOOST_CHECK_CLOSE(read<double>("1.5"), 1.5, 1e-9);
    BOOST_CHECK_CLOSE(read<double>("-2"), -2.0, 1e-9);
    BOOST_CHECK_CLOSE(read<double>("1e3"), 1000.0, 1e-9);
    BOOST_CHECK_CLOSE(read<float>("0.25"), 0.25f, 1e-6f);

    double out;
    BOOST_CHECK(!reads("1.5.5", &out));
    BOOST_CHECK(!reads("", &out));
    BOOST_CHECK(!reads("abc", &out));
    BOOST_CHECK(!reads("1.5x", &out));
    // Beyond what a double can hold, rather than silently infinite.
    BOOST_CHECK(!reads("1e400", &out));
}

BOOST_AUTO_TEST_CASE(test_booleans_spell_themselves_several_ways) {
    for (auto token : {"true", "TRUE", "True", "yes", "on", "1"}) {
        BOOST_CHECK_MESSAGE(read<bool>(token), token);
    }
    for (auto token : {"false", "FALSE", "no", "off", "0"}) {
        BOOST_CHECK_MESSAGE(!read<bool>(token), token);
    }

    bool out;
    BOOST_CHECK(!reads("", &out));
    BOOST_CHECK(!reads("2", &out));
    BOOST_CHECK(!reads("maybe", &out));
}

BOOST_AUTO_TEST_CASE(test_a_caller_can_add_a_type) {
    auto half = read<Fraction>("1/2");
    BOOST_CHECK_EQUAL(half.numerator, 1);
    BOOST_CHECK_EQUAL(half.denominator, 2);

    Fraction out;
    BOOST_CHECK(!reads("1", &out));
    BOOST_CHECK(!reads("1/x", &out));

    BOOST_CHECK_EQUAL(std::string(value_traits<Fraction>::type_name()), "fraction");
}

BOOST_AUTO_TEST_CASE(test_type_info_carries_the_check_without_a_template) {
    // What Argument stores, so that it can hold a type without becoming one.
    auto info = detail::type_info_for<int>();
    BOOST_REQUIRE(info.check != nullptr);
    BOOST_CHECK(info.check("42"));
    BOOST_CHECK(!info.check("x"));
    BOOST_CHECK_EQUAL(std::string(info.name), "int");

    auto text = detail::type_info_for<std::string>();
    BOOST_CHECK(text.check("anything at all"));
}

BOOST_AUTO_TEST_SUITE_END()
