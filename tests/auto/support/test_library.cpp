#include <filesystem>
#include <utility>

#include <stdcorelib/support/sharedlibrary.h>

#include <boost/test/unit_test.hpp>

namespace fs = std::filesystem;

using namespace stdc;

BOOST_AUTO_TEST_SUITE(test_library)

namespace {

    // A library that is guaranteed to be present, plus a symbol it is guaranteed to export.
    // Returns an empty path when the platform has no dependable candidate, in which case the
    // cases that need a real library are skipped rather than failed.
    struct Candidate {
        fs::path path;
        const char *symbol;
    };

    Candidate system_library() {
#if defined(_WIN32)
        for (const char *p :
             {"C:\\Windows\\System32\\kernel32.dll", "C:\\WINNT\\System32\\kernel32.dll"}) {
            if (fs::exists(p)) {
                return {p, "GetProcessHeap"};
            }
        }
#elif defined(__APPLE__)
        for (const char *p : {"/usr/lib/libSystem.B.dylib"}) {
            if (fs::exists(p)) {
                return {p, "malloc"};
            }
        }
#else
        for (const char *p : {"/lib/x86_64-linux-gnu/libm.so.6", "/usr/lib/libm.so.6",
                              "/lib64/libm.so.6", "/usr/lib64/libm.so.6"}) {
            if (fs::exists(p)) {
                return {p, "cos"};
            }
        }
#endif
        return {};
    }

}

BOOST_AUTO_TEST_CASE(test_default_state) {
    SharedLibrary lib;
    BOOST_CHECK(!lib.isOpen());
    BOOST_CHECK(lib.handle() == nullptr);
    BOOST_CHECK(lib.path().empty());
}

BOOST_AUTO_TEST_CASE(test_open_failure) {
    SharedLibrary lib;

    BOOST_CHECK(!lib.open("no_such_library_9f3a.dll"));
    BOOST_CHECK(!lib.isOpen());
    BOOST_CHECK(lib.path().empty()); // a failed open leaves no path behind
    BOOST_CHECK(!lib.lastError().empty());

    // resolving on a closed library yields nothing instead of crashing
    BOOST_CHECK(lib.resolve("anything") == nullptr);
}

BOOST_AUTO_TEST_CASE(test_system_library) {
    auto candidate = system_library();
    if (candidate.path.empty()) {
        BOOST_TEST_MESSAGE("no dependable system library on this platform, skipping");
        return;
    }

    SharedLibrary lib;
    BOOST_REQUIRE(lib.open(candidate.path));
    BOOST_CHECK(lib.isOpen());
    BOOST_CHECK(lib.handle() != nullptr);

    // the recorded path is canonical and points at the file that was opened
    BOOST_CHECK(!lib.path().empty());
    BOOST_CHECK(lib.path().is_absolute());
    BOOST_CHECK(fs::exists(lib.path()));
    BOOST_CHECK(fs::equivalent(lib.path(), candidate.path));

    // an exported symbol resolves, a made-up one does not
    BOOST_CHECK(lib.resolve(candidate.symbol) != nullptr);
    BOOST_CHECK(lib.resolve("no_such_symbol_9f3a") == nullptr);

    BOOST_CHECK(lib.close());
    BOOST_CHECK(!lib.isOpen());
    BOOST_CHECK(lib.path().empty());
}

BOOST_AUTO_TEST_CASE(test_reopen) {
    auto candidate = system_library();
    if (candidate.path.empty()) {
        return;
    }

    // opening an already-open library closes the previous one first
    SharedLibrary lib;
    BOOST_REQUIRE(lib.open(candidate.path));
    auto first = lib.handle();
    BOOST_REQUIRE(lib.open(candidate.path));
    BOOST_CHECK(lib.isOpen());
    BOOST_CHECK(lib.resolve(candidate.symbol) != nullptr);
    (void) first;

    // opening the same library from two objects is fine, and both resolve
    SharedLibrary other;
    BOOST_REQUIRE(other.open(candidate.path));
    BOOST_CHECK(other.resolve(candidate.symbol) != nullptr);
    BOOST_CHECK(lib.resolve(candidate.symbol) != nullptr);
}

BOOST_AUTO_TEST_CASE(test_move) {
    auto candidate = system_library();
    if (candidate.path.empty()) {
        return;
    }

    // move construct: the destination takes over the handle
    // NOTE: the source holds a pimpl pointer that the move leaves null, so a moved-from
    // SharedLibrary must not be touched again, not even to ask isOpen().
    {
        SharedLibrary source;
        BOOST_REQUIRE(source.open(candidate.path));
        auto handle = source.handle();
        auto path = source.path();

        SharedLibrary moved(std::move(source));
        BOOST_CHECK(moved.isOpen());
        BOOST_CHECK_EQUAL(moved.handle(), handle);
        BOOST_CHECK(moved.path() == path);
        BOOST_CHECK(moved.resolve(candidate.symbol) != nullptr);
    }

    // move assign
    {
        SharedLibrary source;
        BOOST_REQUIRE(source.open(candidate.path));
        auto handle = source.handle();

        SharedLibrary target;
        target = std::move(source);
        BOOST_CHECK(target.isOpen());
        BOOST_CHECK_EQUAL(target.handle(), handle);
        BOOST_CHECK(target.resolve(candidate.symbol) != nullptr);
    }
}

BOOST_AUTO_TEST_CASE(test_release) {
    auto candidate = system_library();
    if (candidate.path.empty()) {
        return;
    }

    // release() gives up ownership: close() then forgets the handle without unloading
    SharedLibrary lib;
    BOOST_REQUIRE(lib.open(candidate.path));
    lib.release();
    BOOST_CHECK(lib.close());
    BOOST_CHECK(!lib.isOpen());
    BOOST_CHECK(lib.path().empty());

    // the library is still loaded, so opening it again works
    SharedLibrary again;
    BOOST_CHECK(again.open(candidate.path));
    BOOST_CHECK(again.resolve(candidate.symbol) != nullptr);
}

// isLibrary() is a name check: it never looks at the filesystem.
BOOST_AUTO_TEST_CASE(test_is_library) {
#if defined(_WIN32)
    BOOST_CHECK(SharedLibrary::isLibrary("foo.dll"));
    BOOST_CHECK(SharedLibrary::isLibrary("C:\\dir\\foo.DLL")); // case insensitive
    BOOST_CHECK(!SharedLibrary::isLibrary("foo.so"));
    BOOST_CHECK(!SharedLibrary::isLibrary("foo.exe"));
    BOOST_CHECK(!SharedLibrary::isLibrary("foo"));
    BOOST_CHECK(!SharedLibrary::isLibrary(""));
#elif defined(__APPLE__)
    BOOST_CHECK(SharedLibrary::isLibrary("foo.dylib"));
    BOOST_CHECK(!SharedLibrary::isLibrary("foo.so"));
    BOOST_CHECK(!SharedLibrary::isLibrary("foo"));
#else
    BOOST_CHECK(SharedLibrary::isLibrary("foo.so"));
    BOOST_CHECK(SharedLibrary::isLibrary("libfoo.so.6"));     // versioned
    BOOST_CHECK(SharedLibrary::isLibrary("libfoo.so.1.2.3")); // multi-part version
    BOOST_CHECK(!SharedLibrary::isLibrary("foo.so.beta"));    // not a version
    BOOST_CHECK(!SharedLibrary::isLibrary("foo.dll"));
    BOOST_CHECK(!SharedLibrary::isLibrary("foo"));
#endif
}

BOOST_AUTO_TEST_CASE(test_locate_library_path) {
    auto candidate = system_library();
    if (candidate.path.empty()) {
        return;
    }

    SharedLibrary lib;
    BOOST_REQUIRE(lib.open(candidate.path));

    // an address inside the library maps back to the file it came from
    auto *symbol = lib.resolve(candidate.symbol);
    BOOST_REQUIRE(symbol != nullptr);

    auto located = SharedLibrary::locateLibraryPath(symbol);
    BOOST_CHECK(!located.empty());
    if (!located.empty()) {
        BOOST_CHECK(fs::equivalent(located, candidate.path));
    }
}

BOOST_AUTO_TEST_SUITE_END()
