#include <vector>

#include <stdcorelib/support/library.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_library)

BOOST_AUTO_TEST_CASE(test_system_library) {
    Library lib;
#ifdef _WIN32
    BOOST_CHECK(lib.open(L"C:\\Windows\\System32\\kernel32.dll"));
    BOOST_CHECK(lib.isOpen());
    BOOST_CHECK(lib.resolve("GetProcessHeap"));
#endif
}

BOOST_AUTO_TEST_SUITE_END()