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

// ---------------------------------------------------------------------------------------------
// The builders
// ---------------------------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(test_argument_defaults) {
    Argument arg("file", "The file to read");
    BOOST_CHECK_EQUAL(arg.name(), "file");
    BOOST_CHECK_EQUAL(arg.description(), "The file to read");
    BOOST_CHECK(arg.isRequired());
    BOOST_CHECK(arg.arity() == Argument::Single);
    BOOST_CHECK(!arg.hasDefaultValue());
    BOOST_CHECK(arg.expectedValues().empty());
    BOOST_CHECK(!arg.validator());
    // No type asked for means no check, which is what lets an argument take anything.
    BOOST_CHECK(arg.typeInfo().check == nullptr);
    // Without a metavar the name is what shows.
    BOOST_CHECK_EQUAL(arg.displayName(), "file");
}

BOOST_AUTO_TEST_CASE(test_argument_setters_chain) {
    auto arg = Argument("count", "How many").metavar("N").optional().default_value("1").type<int>();

    BOOST_CHECK_EQUAL(arg.displayName(), "N");
    BOOST_CHECK(!arg.isRequired());
    BOOST_REQUIRE(arg.hasDefaultValue());
    BOOST_CHECK_EQUAL(arg.defaultValue(), "1");

    BOOST_REQUIRE(arg.typeInfo().check != nullptr);
    BOOST_CHECK(arg.typeInfo().check("7"));
    BOOST_CHECK(!arg.typeInfo().check("seven"));
    BOOST_CHECK_EQUAL(std::string(arg.typeInfo().name), "int");
}

BOOST_AUTO_TEST_CASE(test_argument_arity) {
    BOOST_CHECK(Argument("x").arity() == Argument::Single);
    BOOST_CHECK(Argument("x").multi().arity() == Argument::Multiple);
    BOOST_CHECK(Argument("x").multi(false).arity() == Argument::Single);
    BOOST_CHECK(Argument("x").nargs(Argument::Remainder).arity() == Argument::Remainder);
}

BOOST_AUTO_TEST_CASE(test_argument_carries_a_validator_and_a_set_of_words) {
    auto arg = Argument("mode")
                   .expect({"fast", "slow"})
                   .validate([](std::string_view token, std::string *error) {
                       if (token == "slow") {
                           *error = "too slow";
                           return false;
                       }
                       return true;
                   });

    BOOST_CHECK(arg.expectedValues() == std::vector<std::string>({"fast", "slow"}));

    std::string error;
    BOOST_REQUIRE(arg.validator());
    BOOST_CHECK(arg.validator()("fast", &error));
    BOOST_CHECK(error.empty());
    BOOST_CHECK(!arg.validator()("slow", &error));
    BOOST_CHECK_EQUAL(error, "too slow");
}

BOOST_AUTO_TEST_CASE(test_option_is_built_several_ways) {
    Option from_list({"-f", "--force"}, "Force it");
    BOOST_CHECK(from_list.tokens() == std::vector<std::string>({"-f", "--force"}));
    BOOST_CHECK_EQUAL(from_list.token(), "-f");
    BOOST_CHECK_EQUAL(from_list.description(), "Force it");

    Option from_one("--force");
    BOOST_CHECK(from_one.tokens() == std::vector<std::string>({"--force"}));

    Option from_vector(std::vector<std::string>{"-e", "--exclude"});
    BOOST_CHECK_EQUAL(from_vector.tokens().size(), 2u);

    // Defaults, so that what a bare option means is written down somewhere.
    BOOST_CHECK(!from_list.isRequired());
    BOOST_CHECK(!from_list.isGlobal());
    BOOST_CHECK(from_list.role() == Option::NoRole);
    BOOST_CHECK(from_list.prior() == Option::NoPrior);
    BOOST_CHECK(from_list.shortMatch() == Option::NoShortMatch);
    BOOST_CHECK_EQUAL(from_list.maxOccurrence(), 1);
    BOOST_CHECK(from_list.arguments().empty());
}

BOOST_AUTO_TEST_CASE(test_option_roles_bring_their_own_spelling) {
    Option help = Option::Help;
    BOOST_CHECK(help.role() == Option::Help);
    BOOST_CHECK(help.tokens() == std::vector<std::string>({"-h", "--help"}));

    BOOST_CHECK(Option(Option::Version).tokens() == std::vector<std::string>({"-v", "--version"}));
    BOOST_CHECK(Option(Option::Verbose).tokens() == std::vector<std::string>({"-V", "--verbose"}));
    BOOST_CHECK(Option(Option::Debug).tokens() == std::vector<std::string>({"-d", "--debug"}));

    // Naming the tokens keeps the role and drops the usual spelling.
    Option renamed(Option::Help, {"--usage"}, "Print usage");
    BOOST_CHECK(renamed.role() == Option::Help);
    BOOST_CHECK(renamed.tokens() == std::vector<std::string>({"--usage"}));
    BOOST_CHECK_EQUAL(renamed.description(), "Print usage");

    // A role with no spelling of its own gets none.
    BOOST_CHECK(Option(Option::NoRole).tokens().empty());
}

BOOST_AUTO_TEST_CASE(test_option_setters_chain) {
    auto opt = Option({"-D", "--define"}, "Define a variable")
                   .arg("expr")
                   .multi()
                   .short_match(Option::ShortMatchSingleChar)
                   .prior(Option::IgnoreMissingArguments)
                   .required()
                   .global();

    BOOST_REQUIRE_EQUAL(opt.arguments().size(), 1u);
    BOOST_CHECK_EQUAL(opt.arguments().front().name(), "expr");
    BOOST_CHECK(opt.arguments().front().isRequired());
    // An option's argument has the option's description to stand on and needs none of its own.
    BOOST_CHECK(opt.arguments().front().description().empty());

    BOOST_CHECK_EQUAL(opt.maxOccurrence(), 0);
    BOOST_CHECK(opt.shortMatch() == Option::ShortMatchSingleChar);
    BOOST_CHECK(opt.prior() == Option::IgnoreMissingArguments);
    BOOST_CHECK(opt.isRequired());
    BOOST_CHECK(opt.isGlobal());

    // An optional argument, and one built up on its own.
    auto with_optional = Option("-w").arg("file", false);
    BOOST_CHECK(!with_optional.arguments().front().isRequired());

    auto with_typed = Option("-n").arg(Argument("count").type<int>());
    BOOST_CHECK(with_typed.arguments().front().typeInfo().check("3"));
}

BOOST_AUTO_TEST_CASE(test_prior_is_a_ladder) {
    // The parser picks the highest level given rather than switching on each, so the order these
    // are declared in is part of what they mean.
    BOOST_CHECK(Option::NoPrior < Option::IgnoreMissingArguments);
    BOOST_CHECK(Option::IgnoreMissingArguments < Option::IgnoreMissingSymbols);
    BOOST_CHECK(Option::IgnoreMissingSymbols < Option::AutoSetWhenNoSymbols);
    BOOST_CHECK(Option::AutoSetWhenNoSymbols < Option::ExclusiveToArguments);
    BOOST_CHECK(Option::ExclusiveToArguments < Option::ExclusiveToOptions);
    BOOST_CHECK(Option::ExclusiveToOptions < Option::ExclusiveToAll);
}

BOOST_AUTO_TEST_CASE(test_command_collects_what_it_is_given) {
    auto command = Command("copy", "Copy files")
                       .addArguments({
                           Argument("src", "Source").multi(),
                           Argument("dest", "Destination"),
                       })
                       .addOptions({
                           Option({"-e", "--exclude"}, "Exclude a pattern").arg("regex").multi(),
                           Option({"-f", "--force"}, "Force overwrite"),
                       })
                       .addOption({Option::Verbose})
                       .setHandler([](const ParseResult &) { return 7; });

    BOOST_CHECK_EQUAL(command.name(), "copy");
    BOOST_CHECK_EQUAL(command.description(), "Copy files");
    BOOST_REQUIRE_EQUAL(command.arguments().size(), 2u);
    BOOST_CHECK_EQUAL(command.arguments()[0].name(), "src");
    BOOST_CHECK(command.arguments()[0].arity() == Argument::Multiple);
    BOOST_REQUIRE_EQUAL(command.options().size(), 3u);
    BOOST_CHECK(command.options()[2].role() == Option::Verbose);
    BOOST_REQUIRE(command.handler());
}

BOOST_AUTO_TEST_CASE(test_command_addition_appends_rather_than_replaces) {
    // Called twice, which is how corecmd adds the common options after the specific ones.
    Command command("x");
    command.addOption(Option("-a")).addOptions({Option("-b"), Option("-c")});
    BOOST_CHECK_EQUAL(command.options().size(), 3u);

    command.addArgument(Argument("one")).addArguments({Argument("two")});
    BOOST_CHECK_EQUAL(command.arguments().size(), 2u);

    command.addCommand(Command("sub")).addCommands({Command("other")});
    BOOST_CHECK_EQUAL(command.commands().size(), 2u);
}

BOOST_AUTO_TEST_CASE(test_command_lookup) {
    auto command = Command("root")
                       .addOptions({Option({"-f", "--force"}), Option({"-e", "--exclude"})})
                       .addCommands({Command("copy"), Command("rmdir")});

    BOOST_REQUIRE(command.findCommand("copy") != nullptr);
    BOOST_CHECK_EQUAL(command.findCommand("copy")->name(), "copy");
    BOOST_CHECK(command.findCommand("nothing") == nullptr);

    // Any spelling finds it, not only the first.
    BOOST_REQUIRE(command.findOption("-f") != nullptr);
    BOOST_CHECK_EQUAL(command.findOption("--force")->token(), "-f");
    BOOST_CHECK_EQUAL(command.findOption("--exclude")->token(), "-e");
    BOOST_CHECK(command.findOption("-x") == nullptr);
    BOOST_CHECK(command.findOption("") == nullptr);

    // Lookup is one level down, not a search of the whole tree.
    auto nested = Command("outer").addCommand(Command("inner").addCommand(Command("deep")));
    BOOST_CHECK(nested.findCommand("inner") != nullptr);
    BOOST_CHECK(nested.findCommand("deep") == nullptr);
}

BOOST_AUTO_TEST_CASE(test_catalogue_groups_by_heading) {
    CommandCatalogue catalogue;
    BOOST_CHECK(catalogue.isEmpty());

    catalogue.addCommands("Filesystem Commands", {"copy", "rmdir", "touch"})
        .addCommands("Buildsystem Commands", {"configure", "deploy"})
        .addOptions("Common Options", {"-V"});

    BOOST_CHECK(!catalogue.isEmpty());
    BOOST_REQUIRE_EQUAL(catalogue.commandGroups().size(), 2u);
    // Declaration order is the order the headings appear in, so it is kept.
    BOOST_CHECK_EQUAL(catalogue.commandGroups()[0].name, "Filesystem Commands");
    BOOST_CHECK_EQUAL(catalogue.commandGroups()[0].members.size(), 3u);
    BOOST_CHECK_EQUAL(catalogue.commandGroups()[1].name, "Buildsystem Commands");
    BOOST_REQUIRE_EQUAL(catalogue.optionGroups().size(), 1u);
    BOOST_CHECK(catalogue.argumentGroups().empty());

    Command command("root");
    BOOST_CHECK(command.catalogue().isEmpty());
    command.setCatalogue(catalogue);
    BOOST_CHECK_EQUAL(command.catalogue().commandGroups().size(), 2u);
}

BOOST_AUTO_TEST_CASE(test_a_command_tree_copies_whole) {
    // These are values, so handing one around is a copy and not a share. Building a command in a
    // lambda and returning it, which is how corecmd writes every one of them, has to work.
    auto make = [] {
        return Command("copy", "Copy files").addOption(Option("-f")).addArgument(Argument("src"));
    };
    Command original = make();
    Command copy = original;

    copy.addOption(Option("-g"));
    BOOST_CHECK_EQUAL(original.options().size(), 1u);
    BOOST_CHECK_EQUAL(copy.options().size(), 2u);
}

BOOST_AUTO_TEST_SUITE_END()
