#include <stdcorelib/vla.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_vla)

template <int Padding>
class ComplexClass {
public:
    ComplexClass(int i = 0) : i(i) {
        g_count++;
    }
    ComplexClass(const ComplexClass &other) : ComplexClass(other.i) {
    }
    ComplexClass(ComplexClass &&other) : ComplexClass(other.i) {
    }

    ~ComplexClass() {
        g_count--;
    }

public:
    int i;
    char padding[Padding];

    static inline int g_count = 0;
};

BOOST_AUTO_TEST_CASE(test_construct) {
    using CC1 = ComplexClass<4>;

    // construct with size
    {
        vlarray<CC1> v(4);
        BOOST_CHECK_EQUAL(v[2].i, 0);
        BOOST_CHECK_EQUAL(CC1::g_count, 4);
    }
    BOOST_CHECK_EQUAL(CC1::g_count, 0);

    // construct with initializer list
    {
        vlarray<CC1> v = {0, 1, 2, 3};
        BOOST_CHECK_EQUAL(v.size(), 4);
        BOOST_CHECK_EQUAL(v[2].i, 2);
        BOOST_CHECK_EQUAL(CC1::g_count, 4);
    }
    BOOST_CHECK_EQUAL(CC1::g_count, 0);

    // copy construct
    {
        vlarray<CC1, 256> v0 = {0, 1, 2, 3};
        vlarray<CC1> v(v0);
        BOOST_CHECK_EQUAL(v[2].i, 2);
        BOOST_CHECK_EQUAL(CC1::g_count, 8);
    }
    BOOST_CHECK_EQUAL(CC1::g_count, 0);

    // move construct
    {
        vlarray<CC1, 256> v0 = {0, 1, 2, 3};
        vlarray<CC1> v(std::move(v0));
        BOOST_CHECK_EQUAL(v[2].i, 2);
        BOOST_CHECK_EQUAL(CC1::g_count, 8);
    }
    BOOST_CHECK_EQUAL(CC1::g_count, 0);

    // copy assign
    {
        vlarray<CC1, 256> v0 = {0, 1, 2, 3};
        vlarray<CC1> v;
        v = v0;
        BOOST_CHECK_EQUAL(v[2].i, 2);
        BOOST_CHECK_EQUAL(CC1::g_count, 8);
    }
    BOOST_CHECK_EQUAL(CC1::g_count, 0);

    // move assign
    {
        vlarray<CC1, 256> v0 = {0, 1, 2, 3};
        vlarray<CC1> v;
        v = std::move(v0);
        BOOST_CHECK_EQUAL(v[2].i, 2);
        BOOST_CHECK_EQUAL(CC1::g_count, 8);
    }
    BOOST_CHECK_EQUAL(CC1::g_count, 0);

    using CC2 = ComplexClass<1024>;

    // move construct
    {
        vlarray<CC2, 256> v0 = {0, 1, 2, 3};
        vlarray<CC2> v(std::move(v0));
        BOOST_CHECK_EQUAL(v[2].i, 2);
        BOOST_CHECK_EQUAL(CC2::g_count, 4);
    }
    BOOST_CHECK_EQUAL(CC2::g_count, 0);

    // move assign
    {
        vlarray<CC2, 256> v0 = {0, 1, 2, 3};
        vlarray<CC2> v;
        v = std::move(v0);
        BOOST_CHECK_EQUAL(v[2].i, 2);
        BOOST_CHECK_EQUAL(CC2::g_count, 4);
    }
    BOOST_CHECK_EQUAL(CC2::g_count, 0);
}

BOOST_AUTO_TEST_CASE(test_resize) {
    using CC3 = ComplexClass<64>;

    auto &gc = CC3::g_count;

    vlarray<CC3> v = {0, 1, 2, 3};
    BOOST_CHECK_EQUAL(CC3::g_count, 4);

    // stack shrink to stack
    v.resize(2);
    BOOST_CHECK_EQUAL(v.size(), 2);
    BOOST_CHECK_EQUAL(CC3::g_count, 2);

    // stack expand to stack
    v.resize(4);
    BOOST_CHECK_EQUAL(v[3].i, 0);

    // stack expand to heap
    v.resize(20);
    BOOST_CHECK_EQUAL(v[15].i, 0);
    BOOST_CHECK_EQUAL(CC3::g_count, 20);

    // heap shrink to heap
    v.resize(10);
    BOOST_CHECK_EQUAL(v[6].i, 0);
    BOOST_CHECK_EQUAL(CC3::g_count, 10);

    // heap expand to heap
    v.resize(50);
    BOOST_CHECK_EQUAL(v[46].i, 0);
    BOOST_CHECK_EQUAL(CC3::g_count, 50);

    // heap shrink to stack
    v.resize(2);
    BOOST_CHECK_EQUAL(v.size(), 2);
    BOOST_CHECK_EQUAL(CC3::g_count, 2);
}

BOOST_AUTO_TEST_CASE(test_operators) {
    vlarray<int> v1{1, 2, 3, 4};
    vlarray<int> v2{1, 2, 3, 4};
    vlarray<int, 4> v3{1, 2, 3, 4};
    vlarray<int, 4> v4{1, 2, 3};
    BOOST_CHECK(v1 == v2);
    BOOST_CHECK(v2 == v3);
    BOOST_CHECK(v2 != v4);

    v2.resize(3);
    BOOST_CHECK(v2 == v4);

    v4.resize(4);
    v4[3] = 4;
    BOOST_CHECK(v1 == v4);
}

BOOST_AUTO_TEST_SUITE_END()