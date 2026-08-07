// SPDX-License-Identifier: MIT

#define BOOST_TEST_MAIN

#include <boost/test/unit_test.hpp>

#ifdef BOOST_NO_EXCEPTIONS

#  include <cstdio>
#  include <cstdlib>

#  include <boost/assert/source_location.hpp>

// Boost hands the throwing back when it is built without exceptions, and there is exactly one
// thing left to do with one: say what it was and stop. This configuration is a check that the
// library compiles and runs with the feature off, not a suite meant to survive a failure.
namespace boost {

    void throw_exception(const std::exception &e) {
        std::fprintf(stderr, "boost raised \"%s\" in a build with no exceptions\n", e.what());
        std::abort();
    }

    void throw_exception(const std::exception &e, const boost::source_location &where) {
        std::fprintf(stderr, "boost raised \"%s\" at %s:%u in a build with no exceptions\n",
                     e.what(), where.file_name(), where.line());
        std::abort();
    }

}

#endif
