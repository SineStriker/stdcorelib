// SPDX-License-Identifier: MIT

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <stdcorelib/support/staticregistry.h>
#include <stdcorelib/support/dynamicregistry.h>

#include <boost/test/unit_test.hpp>

using namespace stdc;

namespace {

    class Codec {
    public:
        virtual ~Codec() = default;
        virtual std::string id() const = 0;
    };

    class FlacCodec : public Codec {
    public:
        std::string id() const override {
            return "flac";
        }
    };

    class OpusCodec : public Codec {
    public:
        std::string id() const override {
            return "opus";
        }
    };

    // No default constructor, which is what Add<V> cannot reach.
    class PcmCodec : public Codec {
    public:
        PcmCodec(int rate, int channels) : _rate(rate), _channels(channels) {
        }
        std::string id() const override {
            return "pcm-" + std::to_string(_rate) + "-" + std::to_string(_channels);
        }

    private:
        int _rate;
        int _channels;
    };

}

STDCORELIB_INSTANTIATE_STATIC_REGISTRY(Codec)

namespace {

    using CodecRegistry = StaticRegistry<Codec>;

    // The point of the whole design: these run before main, with no initialization call
    // anywhere, and in the order they are written.
    CodecRegistry::Add<FlacCodec> flac_entry("flac", "Free Lossless Audio Codec");
    CodecRegistry::Add<OpusCodec> opus_entry("opus", "Opus Interactive Audio Codec");

    // Constructor arguments, baked in at the call site. A capture-less lambda is a function
    // pointer, so this costs no more than the Add above.
    CodecRegistry::AddFactory pcm_entry("pcm", "Linear PCM", []() -> CodecRegistry::Created {
        return std::unique_ptr<Codec>(new PcmCodec(44100, 2));
    });

}

BOOST_AUTO_TEST_SUITE(test_registries)

BOOST_AUTO_TEST_CASE(test_static_registry_is_filled_before_main) {
    std::vector<std::string> names;
    for (const auto &entry : CodecRegistry::entries()) {
        names.push_back(std::string(entry.name()));
    }

    BOOST_REQUIRE_EQUAL(names.size(), 3u);
    BOOST_CHECK_EQUAL(names[0], "flac"); // registration order, not sorted
    BOOST_CHECK_EQUAL(names[1], "opus");
    BOOST_CHECK_EQUAL(names[2], "pcm");
}

// Add<V> can only default construct. AddFactory is how an implementation that takes arguments
// gets in.
BOOST_AUTO_TEST_CASE(test_static_registry_factory_entry) {
    const CodecRegistry::Entry *pcm = nullptr;
    for (const auto &entry : CodecRegistry::entries()) {
        if (entry.name() == "pcm") {
            pcm = &entry;
        }
    }
    BOOST_REQUIRE(pcm);
    BOOST_CHECK_EQUAL(std::string(pcm->desc()), "Linear PCM");

    auto codec = pcm->instantiate();
    BOOST_REQUIRE(codec);
    BOOST_CHECK_EQUAL(codec->id(), "pcm-44100-2"); // the arguments survived
}

BOOST_AUTO_TEST_CASE(test_static_registry_entries) {
    auto it = CodecRegistry::begin();
    BOOST_CHECK_EQUAL(std::string(it->name()), "flac");
    BOOST_CHECK_EQUAL(std::string(it->desc()), "Free Lossless Audio Codec");

    // each instantiate() is a fresh object, so the registry holds descriptions not instances
    auto first = it->instantiate();
    auto second = it->instantiate();
    BOOST_REQUIRE(first && second);
    BOOST_CHECK(first.get() != second.get());
    BOOST_CHECK_EQUAL(first->id(), "flac");

    BOOST_CHECK(++it != CodecRegistry::end());
    BOOST_CHECK(++it != CodecRegistry::end());
    BOOST_CHECK(++it == CodecRegistry::end());
}

namespace {

    struct Widget {
        virtual ~Widget() = default;
        int tag = 0;
    };

    using WidgetRegistry = DynamicRegistry<Widget>;

    WidgetRegistry::EntryPointer make(const std::string &name, int tag) {
        WidgetRegistry::instance().add(name, name + " widget", [tag] {
            auto w = std::unique_ptr<Widget>(new Widget());
            w->tag = tag;
            return w;
        });
        return WidgetRegistry::instance().find(name);
    }

    struct RegistryGuard {
        ~RegistryGuard() {
            WidgetRegistry::instance().clear();
        }
    };

    class CountingListener : public WidgetRegistry::Listener {
    public:
        void entry_added(const WidgetRegistry::EntryPointer &entry) override {
            added.push_back(entry->name());
        }
        void entry_removed(const WidgetRegistry::EntryPointer &entry) override {
            removed.push_back(entry->name());
        }
        std::vector<std::string> added, removed;
    };

}

BOOST_AUTO_TEST_CASE(test_dynamic_registry_add_find_remove) {
    RegistryGuard guard;
    auto &reg = WidgetRegistry::instance();

    BOOST_CHECK(make("alpha", 1) != nullptr);
    BOOST_CHECK_EQUAL(reg.size(), 1u);

    // a name is taken only once
    BOOST_CHECK(!reg.add("alpha", "again", [] { return std::unique_ptr<Widget>(new Widget()); }));
    BOOST_CHECK_EQUAL(reg.size(), 1u);

    auto entry = reg.find("alpha");
    BOOST_REQUIRE(entry);
    BOOST_CHECK_EQUAL(entry->name(), "alpha");
    BOOST_CHECK_EQUAL(entry->desc(), "alpha widget");
    BOOST_CHECK_EQUAL(entry->instantiate()->tag, 1);

    // instantiate() by name is the same thing without the lookup in between
    auto direct = reg.instantiate("alpha");
    BOOST_REQUIRE(direct);
    BOOST_CHECK_EQUAL(direct->tag, 1);

    BOOST_CHECK(!reg.find("nothing"));
    BOOST_CHECK(!reg.instantiate("nothing"));

    BOOST_CHECK(reg.remove("alpha"));
    BOOST_CHECK(!reg.remove("alpha"));
    BOOST_CHECK(!reg.find("alpha"));

    // the entry we are still holding outlives its removal, which is why they are shared
    BOOST_CHECK_EQUAL(entry->instantiate()->tag, 1);
}

BOOST_AUTO_TEST_CASE(test_dynamic_registry_entries_are_sorted) {
    RegistryGuard guard;
    make("gamma", 3);
    make("alpha", 1);
    make("beta", 2);

    auto entries = WidgetRegistry::instance().entries();
    BOOST_REQUIRE_EQUAL(entries.size(), 3u);
    BOOST_CHECK_EQUAL(entries[0]->name(), "alpha");
    BOOST_CHECK_EQUAL(entries[1]->name(), "beta");
    BOOST_CHECK_EQUAL(entries[2]->name(), "gamma");
}

BOOST_AUTO_TEST_CASE(test_dynamic_registry_listeners) {
    RegistryGuard guard;
    auto &reg = WidgetRegistry::instance();

    CountingListener listener;
    reg.add_listener(&listener);
    reg.add_listener(&listener); // twice is once

    make("one", 1);
    reg.remove("one");

    BOOST_REQUIRE_EQUAL(listener.added.size(), 1u);
    BOOST_CHECK_EQUAL(listener.added[0], "one");
    BOOST_REQUIRE_EQUAL(listener.removed.size(), 1u);
    BOOST_CHECK_EQUAL(listener.removed[0], "one");

    reg.remove_listener(&listener);
    make("two", 2);
    BOOST_CHECK_EQUAL(listener.added.size(), 1u); // no longer told
}

// The registry takes a lock, so registering from several threads at once has to come out with
// every entry present and none of them duplicated.
BOOST_AUTO_TEST_CASE(test_dynamic_registry_is_thread_safe) {
    RegistryGuard guard;
    auto &reg = WidgetRegistry::instance();

    constexpr int Threads = 8;
    constexpr int PerThread = 50;

    std::vector<std::thread> workers;
    for (int t = 0; t < Threads; ++t) {
        workers.emplace_back([t, PerThread, &reg] {
            for (int i = 0; i < PerThread; ++i) {
                auto name = std::to_string(t) + "." + std::to_string(i);
                reg.add(name, {}, [] { return std::unique_ptr<Widget>(new Widget()); });
                std::ignore = reg.find(name);
                std::ignore = reg.entries();
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }

    BOOST_CHECK_EQUAL(reg.size(), size_t(Threads * PerThread));
}

BOOST_AUTO_TEST_SUITE_END()
