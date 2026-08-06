// SPDX-License-Identifier: MIT

#include <cstdint>
#include <filesystem>
#include <fstream>
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
    // Called twice, which is how a program adds its common options after the specific ones.
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
    // lambda and returning it, which is how a tree of any size gets built, has to work.
    auto make = [] {
        return Command("copy", "Copy files").addOption(Option("-f")).addArgument(Argument("src"));
    };
    Command original = make();
    Command copy = original;

    copy.addOption(Option("-g"));
    BOOST_CHECK_EQUAL(original.options().size(), 1u);
    BOOST_CHECK_EQUAL(copy.options().size(), 2u);
}

// ---------------------------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------------------------

namespace {

    /// The program name the shell puts in front, which the parser is expected to step over.
    std::vector<std::string> argv(std::initializer_list<std::string> rest) {
        std::vector<std::string> res{"prog"};
        res.insert(res.end(), rest.begin(), rest.end());
        return res;
    }

    /// A parse that is expected to succeed, so that a failing one says why rather than blowing
    /// up somewhere further down.
    ParseResult ok(const Parser &parser, std::initializer_list<std::string> args,
                   int flags = Parser::Standard) {
        auto result = parser.parse(argv(args), flags);
        BOOST_REQUIRE_MESSAGE(result.isValid(), result.errorText());
        return result;
    }

    ParseResult bad(const Parser &parser, std::initializer_list<std::string> args,
                    ParseResult::Error expected, int flags = Parser::Standard) {
        auto result = parser.parse(argv(args), flags);
        BOOST_REQUIRE_MESSAGE(!result.isValid(), "expected a failure, got a clean parse");
        BOOST_CHECK_EQUAL(int(result.error()), int(expected));
        // Whatever went wrong, it has to be sayable.
        BOOST_CHECK_MESSAGE(!result.errorText().empty(), "the failure came with nothing to print");
        return result;
    }

}

BOOST_AUTO_TEST_CASE(test_parse_bare_command) {
    Parser parser(Command("prog", "A program"));
    auto result = ok(parser, {});
    BOOST_REQUIRE(result.command() != nullptr);
    BOOST_CHECK_EQUAL(result.command()->name(), "prog");
    BOOST_CHECK(result.commandPath() == std::vector<std::string>({"prog"}));
}

BOOST_AUTO_TEST_CASE(test_positional_arguments) {
    Parser parser(Command("prog").addArguments({Argument("src"), Argument("dest")}));

    auto result = ok(parser, {"a", "b"});
    BOOST_CHECK_EQUAL(result.value(0), "a");
    BOOST_CHECK_EQUAL(result.value(1), "b");

    // Reading past the end is empty rather than a crash, which is what lets a caller read an
    // optional argument without asking first.
    BOOST_CHECK_EQUAL(result.value(2), "");
    BOOST_CHECK_EQUAL(result.value(-1), "");

    bad(parser, {"a"}, ParseResult::MissingCommandArgument);
    bad(parser, {"a", "b", "c"}, ParseResult::TooManyArguments);
}

BOOST_AUTO_TEST_CASE(test_a_multi_argument_leaves_room_for_what_follows) {
    // copy <src>... <dest>, which only works if the greedy one stops one short.
    Parser parser(
        Parser(Command("prog").addArguments({Argument("src").multi(), Argument("dest")})));

    auto result = ok(parser, {"one", "two", "three", "out"});
    BOOST_CHECK(result.values(0) == std::vector<std::string_view>({"one", "two", "three"}));
    BOOST_CHECK_EQUAL(result.value(1), "out");

    // Two tokens is one each.
    auto pair = ok(parser, {"one", "out"});
    BOOST_CHECK(pair.values(0) == std::vector<std::string_view>({"one"}));
    BOOST_CHECK_EQUAL(pair.value(1), "out");

    bad(parser, {"only"}, ParseResult::MissingCommandArgument);
}

BOOST_AUTO_TEST_CASE(test_remainder_takes_everything_left) {
    Parser parser(Command("prog").addArguments(
        {Argument("script"), Argument("args").nargs(Argument::Remainder).optional()}));

    auto result = ok(parser, {"run.sh", "one", "two"});
    BOOST_CHECK_EQUAL(result.value(0), "run.sh");
    BOOST_CHECK(result.values(1) == std::vector<std::string_view>({"one", "two"}));
}

BOOST_AUTO_TEST_CASE(test_default_value_stands_in) {
    Parser parser(Command("prog").addArgument(
        Argument("level", "How loud", false).default_value("3").type<int>()));

    BOOST_CHECK_EQUAL(ok(parser, {}).value<int>(0), 3);
    BOOST_CHECK_EQUAL(ok(parser, {"7"}).value<int>(0), 7);
}

BOOST_AUTO_TEST_CASE(test_options_in_their_several_spellings) {
    Parser parser(
        Command("prog").addOptions({Option({"-f", "--force"}, "Force"),
                                    Option({"-o", "--out"}, "Where to write").arg("dir")}));

    BOOST_CHECK(ok(parser, {"-f"}).isOptionSet("-f"));
    // Any spelling is the same option, whichever was typed and whichever is asked for.
    BOOST_CHECK(ok(parser, {"--force"}).isOptionSet("-f"));
    BOOST_CHECK(ok(parser, {"-f"}).isOptionSet("--force"));
    BOOST_CHECK(!ok(parser, {}).isOptionSet("-f"));

    BOOST_CHECK_EQUAL(ok(parser, {"-o", "build"}).valueForOption("-o"), "build");
    BOOST_CHECK_EQUAL(ok(parser, {"--out=build"}).valueForOption("--out"), "build");

    bad(parser, {"-x"}, ParseResult::UnknownOption);
    bad(parser, {"-o"}, ParseResult::MissingOptionArgument);
    // An option that takes nothing cannot be given something.
    bad(parser, {"--force=yes"}, ParseResult::UnknownOption);
}

BOOST_AUTO_TEST_CASE(test_repeated_options) {
    Parser parser(Command("prog").addOptions({
        Option({"-e", "--exclude"}, "Exclude").arg("pattern").multi(),
        Option({"-f"}, "Force"),
    }));

    auto result = ok(parser, {"-e", "a", "-e", "b", "-e", "c"});
    BOOST_CHECK_EQUAL(result.option("-e").count(), 3);
    BOOST_CHECK(result.option("-e").rawValues() == std::vector<std::string_view>({"a", "b", "c"}));
    // Each occurrence is still reachable on its own.
    BOOST_CHECK_EQUAL(result.option("-e").rawValue(0, 1), "b");

    // One that did not say it repeats does not.
    bad(parser, {"-f", "-f"}, ParseResult::OptionOccurTooMuch);
}

BOOST_AUTO_TEST_CASE(test_short_match_joins_a_value_to_its_option) {
    Parser parser(Command("prog").addOptions({
        Option({"-D", "--define"}, "Define")
            .arg("expr")
            .multi()
            .short_match(Option::ShortMatchSingleChar),
        Option({"-p"}, "Plain").arg("value"),
    }));

    auto result = ok(parser, {"-DKEY=VALUE"});
    BOOST_CHECK_EQUAL(result.valueForOption("-D"), "KEY=VALUE");

    // Separately still works, and both forms mix.
    auto mixed = ok(parser, {"-D", "A=1", "-DB=2"});
    BOOST_CHECK(mixed.option("-D").rawValues() == std::vector<std::string_view>({"A=1", "B=2"}));

    // An option that did not ask for short matching does not get it.
    bad(parser, {"-pvalue"}, ParseResult::UnknownOption);
}

BOOST_AUTO_TEST_CASE(test_short_match_rules_differ) {
    auto build = [](Option::ShortMatch rule) {
        return Parser(Command("prog").addOption(
            Option({"-1", "--one"}, "Numeric token").arg("value").short_match(rule)));
    };

    // A single letter means a letter, so an option spelled with a digit is not matched.
    BOOST_CHECK(!build(Option::ShortMatchSingleLetter).parse(argv({"-1x"})).isValid());
    // A single character does not care what the character is.
    BOOST_CHECK(build(Option::ShortMatchSingleChar).parse(argv({"-1x"})).isValid());

    // A longer token only matches under the rule that allows any length.
    Parser strict(Command("prog").addOption(
        Option({"--jobs"}, "How many").arg("n").short_match(Option::ShortMatchSingleChar)));
    BOOST_CHECK(!strict.parse(argv({"--jobs8"})).isValid());

    Parser loose(Command("prog").addOption(
        Option({"--jobs"}, "How many").arg("n").short_match(Option::ShortMatchAll)));
    BOOST_CHECK_EQUAL(ok(loose, {"--jobs8"}).valueForOption("--jobs"), "8");
}

BOOST_AUTO_TEST_CASE(test_grouped_short_flags) {
    Parser parser(Command("prog").addOptions({
        Option({"-a"}, "A"),
        Option({"-b"}, "B"),
        Option({"-c"}, "C").arg("value"),
    }));

    auto result = ok(parser, {"-ab"}, Parser::AllowUnixGroupFlags);
    BOOST_CHECK(result.isOptionSet("-a"));
    BOOST_CHECK(result.isOptionSet("-b"));

    // Off by default, so the same line is one unknown option.
    bad(parser, {"-ab"}, ParseResult::UnknownOption);

    // A letter that wants a value cannot be in the middle of a group, so the group is refused
    // whole rather than half taken.
    bad(parser, {"-abc"}, ParseResult::UnknownOption, Parser::AllowUnixGroupFlags);
}

BOOST_AUTO_TEST_CASE(test_double_dash_ends_the_options) {
    Parser parser(
        Command("prog").addArgument(Argument("files").multi()).addOption(Option({"-f"}, "Force")));

    auto result = ok(parser, {"-f", "--", "-f", "--not-an-option"});
    BOOST_CHECK(result.isOptionSet("-f"));
    BOOST_CHECK_EQUAL(result.option("-f").count(), 1);
    BOOST_CHECK(result.values(0) == std::vector<std::string_view>({"-f", "--not-an-option"}));
}

BOOST_AUTO_TEST_CASE(test_subcommands) {
    Parser parser(Command("prog").addCommands({
        Command("copy", "Copy").addArgument(Argument("src")),
        Command("remove", "Remove").addArgument(Argument("path")),
    }));

    auto result = ok(parser, {"copy", "a"});
    BOOST_REQUIRE(result.command() != nullptr);
    BOOST_CHECK_EQUAL(result.command()->name(), "copy");
    BOOST_CHECK(result.commandPath() == std::vector<std::string>({"prog", "copy"}));
    BOOST_CHECK_EQUAL(result.value(0), "a");

    // A name that is no subcommand, on a command that takes no arguments either, is named as
    // the command it is not rather than counted as an argument too many.
    auto failure = bad(parser, {"nonsense"}, ParseResult::UnknownCommand);
    BOOST_CHECK(failure.errorText().find("nonsense") != std::string::npos);

    // Where the command does take arguments, a name that is not a subcommand is one of those,
    // and only the surplus is counted.
    Parser mixed(Command("prog").addArgument(Argument("a")).addCommand(Command("copy")));
    BOOST_CHECK_EQUAL(ok(mixed, {"nonsense"}).value(0), "nonsense");
    bad(mixed, {"one", "two"}, ParseResult::TooManyArguments);
}

BOOST_AUTO_TEST_CASE(test_nested_subcommands) {
    Parser parser(Command("prog").addCommand(
        Command("remote").addCommand(Command("add").addArgument(Argument("name")))));

    auto result = ok(parser, {"remote", "add", "origin"});
    BOOST_CHECK(result.commandPath() == std::vector<std::string>({"prog", "remote", "add"}));
    BOOST_CHECK_EQUAL(result.value(0), "origin");
}

BOOST_AUTO_TEST_CASE(test_global_options_reach_subcommands) {
    Parser parser(Command("prog")
                      .addOption(Option({"-V", "--verbose"}, "Talk more").global())
                      .addOption(Option({"-q"}, "Local to the root"))
                      .addCommand(Command("copy")));

    auto result = ok(parser, {"copy", "-V"});
    BOOST_CHECK(result.isOptionSet("-V"));

    // One that is not global stays where it was declared.
    bad(parser, {"copy", "-q"}, ParseResult::UnknownOption);
}

BOOST_AUTO_TEST_CASE(test_required_option) {
    Parser parser(Command("prog").addOption(Option({"-o"}, "Out").arg("dir").required()));

    BOOST_CHECK(ok(parser, {"-o", "x"}).isOptionSet("-o"));
    bad(parser, {}, ParseResult::MissingRequiredOption);
}

BOOST_AUTO_TEST_CASE(test_a_declared_type_is_checked_while_parsing) {
    Parser parser(Command("prog")
                      .addArgument(Argument("count").type<int>())
                      .addOption(Option({"-r"}, "Ratio").arg(Argument("value").type<double>())));

    BOOST_CHECK_EQUAL(ok(parser, {"12"}).value<int>(0), 12);
    BOOST_CHECK_CLOSE(ok(parser, {"1", "-r", "0.5"}).valueForOption<double>("-r"), 0.5, 1e-9);

    // The point of declaring the type: a bad token is a diagnostic, not a zero read later.
    auto failure = bad(parser, {"twelve"}, ParseResult::ArgumentTypeMismatch);
    BOOST_CHECK(failure.errorText().find("int") != std::string::npos);
    bad(parser, {"1", "-r", "half"}, ParseResult::ArgumentTypeMismatch);
}

BOOST_AUTO_TEST_CASE(test_expected_values_and_validators) {
    Parser parser(Command("prog")
                      .addArgument(Argument("mode").expect({"fast", "slow"}))
                      .addOption(Option({"-n"}, "Name")
                                     .arg(Argument("value").validate(
                                         [](std::string_view token, std::string *error) {
                                             if (token.empty()) {
                                                 *error = "a name cannot be empty";
                                                 return false;
                                             }
                                             return true;
                                         }))));

    BOOST_CHECK_EQUAL(ok(parser, {"fast"}).value(0), "fast");
    bad(parser, {"medium"}, ParseResult::InvalidArgumentValue);

    auto failure = bad(parser, {"fast", "-n", ""}, ParseResult::ArgumentValidateFailed);
    // What the validator said is what gets printed, rather than a generic complaint.
    BOOST_CHECK_EQUAL(failure.errorText(), "a name cannot be empty");
}

BOOST_AUTO_TEST_CASE(test_prior_lets_help_answer_an_incomplete_line) {
    Parser parser(Command("prog")
                      .addArgument(Argument("required one"))
                      .addOption(Option(Option::Help).prior(Option::IgnoreMissingSymbols)));

    // Without the prior level this is a missing argument.
    BOOST_CHECK(!parser.parse(argv({})).isValid());
    auto result = ok(parser, {"--help"});
    BOOST_CHECK(result.isRoleSet(Option::Help));
    // The role is what a caller asks about, not the spelling.
    BOOST_CHECK(!result.isRoleSet(Option::Version));
}

BOOST_AUTO_TEST_CASE(test_prior_can_set_itself_on_an_empty_line) {
    Parser parser(Command("prog")
                      .addArgument(Argument("required one"))
                      .addOption(Option(Option::Help).prior(Option::AutoSetWhenNoSymbols)));

    auto result = ok(parser, {});
    BOOST_CHECK(result.isOptionSet("--help"));

    // Given anything at all it stays out of the way.
    BOOST_CHECK(!ok(parser, {"value"}).isOptionSet("--help"));
}

BOOST_AUTO_TEST_CASE(test_exclusive_prior_levels) {
    auto build = [](Option::Prior level) {
        return Parser(Command("prog")
                          .addArgument(Argument("path").optional())
                          .addOption(Option({"-f"}, "Force"))
                          .addOption(Option(Option::Version).prior(level)));
    };

    // Alone it is fine either way.
    BOOST_CHECK(build(Option::ExclusiveToAll).parse(argv({"--version"})).isValid());

    // With an argument beside it, only the levels that forbid arguments complain.
    BOOST_CHECK(build(Option::ExclusiveToOptions).parse(argv({"--version", "x"})).isValid());
    auto with_argument = build(Option::ExclusiveToArguments).parse(argv({"--version", "x"}));
    BOOST_CHECK_EQUAL(int(with_argument.error()), int(ParseResult::PriorOptionWithArguments));

    // With another option beside it, likewise.
    BOOST_CHECK(build(Option::ExclusiveToArguments).parse(argv({"--version", "-f"})).isValid());
    auto with_option = build(Option::ExclusiveToAll).parse(argv({"--version", "-f"}));
    BOOST_CHECK_EQUAL(int(with_option.error()), int(ParseResult::PriorOptionWithOptions));
}

BOOST_AUTO_TEST_CASE(test_an_option_may_ignore_its_own_missing_arguments) {
    Parser parser(Command("prog").addOption(
        Option({"-l"}, "List").arg("what").prior(Option::IgnoreMissingArguments)));

    auto result = ok(parser, {"-l"});
    BOOST_CHECK(result.isOptionSet("-l"));
    BOOST_CHECK_EQUAL(result.valueForOption("-l"), "");
}

BOOST_AUTO_TEST_CASE(test_case_insensitivity_is_asked_for) {
    Parser parser(
        Command("prog").addOption(Option({"--force"}, "Force")).addCommand(Command("copy")));

    BOOST_CHECK(ok(parser, {"--FORCE"}, Parser::IgnoreOptionCase).isOptionSet("--force"));
    BOOST_CHECK(!parser.parse(argv({"--FORCE"})).isValid());

    BOOST_CHECK_EQUAL(ok(parser, {"COPY"}, Parser::IgnoreCommandCase).command()->name(), "copy");
    // Without the flag it is not a command, so it falls through to being an argument.
    BOOST_CHECK(!parser.parse(argv({"COPY"})).isValid());
}

BOOST_AUTO_TEST_CASE(test_dos_short_options) {
    Parser parser(Command("prog").addOption(Option({"-f"}, "Force")));

    BOOST_CHECK(ok(parser, {"/f"}, Parser::AllowDosShortOptions).isOptionSet("-f"));
    // Off by default, where it is a positional and the command takes none.
    bad(parser, {"/f"}, ParseResult::TooManyArguments);
}

BOOST_AUTO_TEST_CASE(test_unix_short_options_can_be_turned_off) {
    Parser parser(Command("prog")
                      .addArgument(Argument("path").optional())
                      .addOption(Option({"-f", "--force"}, "Force")));

    BOOST_CHECK(ok(parser, {"-f"}).isOptionSet("-f"));
    // With them off a single dash is just a token, which lands in the argument.
    auto result = ok(parser, {"-f"}, Parser::DontAllowUnixShortOptions);
    BOOST_CHECK(!result.isOptionSet("-f"));
    BOOST_CHECK_EQUAL(result.value(0), "-f");
    // The long spelling is untouched.
    BOOST_CHECK(ok(parser, {"--force"}, Parser::DontAllowUnixShortOptions).isOptionSet("-f"));
}

BOOST_AUTO_TEST_CASE(test_invoke_runs_the_command_that_was_reached) {
    std::string seen;
    Parser parser(Command("prog")
                      .addCommand(Command("copy")
                                      .addArgument(Argument("src"))
                                      .setHandler([&seen](const ParseResult &result) {
                                          seen = result.value(0);
                                          return 3;
                                      }))
                      .addCommand(Command("bare")));

    BOOST_CHECK_EQUAL(parser.invoke(argv({"copy", "file"})), 3);
    BOOST_CHECK_EQUAL(seen, "file");

    // No handler, and a parse that failed, both hand back what the caller asked for.
    BOOST_CHECK_EQUAL(parser.invoke(argv({"bare"}), -9), -9);
    BOOST_CHECK_EQUAL(parser.invoke(argv({"copy"}), -9), -9);
}

BOOST_AUTO_TEST_CASE(test_typed_reads) {
    Parser parser(Command("prog")
                      .addArgument(Argument("numbers").multi().type<int>())
                      .addOption(Option({"-n"}, "How many").arg(Argument("n").type<int>())));

    auto result = ok(parser, {"1", "2", "3", "-n", "9"});
    BOOST_CHECK(result.values<int>(0) == std::vector<int>({1, 2, 3}));
    BOOST_CHECK_EQUAL(result.value<int>(0), 1);
    BOOST_CHECK_EQUAL(result.valueForOption<int>("-n"), 9);
    // The untyped read is the text, which is what everything is stored as.
    BOOST_CHECK_EQUAL(result.value(0), "1");
}

// ---------------------------------------------------------------------------------------------
// Edges, taken from what CLI11, argparse and argtable3 test their own parsers with. Their
// semantics are not ours, so what is borrowed is the question each case asks rather than the
// answer it expects.
// ---------------------------------------------------------------------------------------------

// CLI11's DashedOptions and FlagLikeOption ask what happens to a value that looks like a switch.
BOOST_AUTO_TEST_CASE(test_a_value_that_looks_like_an_option) {
    Parser parser(Command("prog")
                      .addOption(Option({"-o"}, "Out").arg("dir"))
                      .addOption(Option({"-f"}, "Force")));

    // A declared option is never quietly taken as somebody else's value. The complaint names
    // the option that went without, which beats swallowing -f and saying nothing.
    bad(parser, {"-o", "-f"}, ParseResult::MissingOptionArgument);

    // Joined to it, it is a value like any other.
    BOOST_CHECK_EQUAL(ok(parser, {"-o=-f"}).valueForOption("-o"), "-f");

    // So is anything that merely looks like an option without being one. A negative number is
    // the case this matters for.
    BOOST_CHECK_EQUAL(ok(parser, {"-o", "-5"}).valueForOption("-o"), "-5");
    BOOST_CHECK_EQUAL(ok(parser, {"-o", "-nonsense"}).valueForOption("-o"), "-nonsense");
}

// CLI11's ForcedPositional, pushed further than the first case in this file does.
BOOST_AUTO_TEST_CASE(test_what_survives_the_terminator) {
    Parser parser(Command("prog")
                      .addArgument(Argument("rest").multi().optional())
                      .addOption(Option({"-f"}, "Force")));

    // A second one is no longer special.
    auto twice = ok(parser, {"--", "--", "-f"});
    BOOST_CHECK(twice.values(0) == std::vector<std::string_view>({"--", "-f"}));
    BOOST_CHECK(!twice.isOptionSet("-f"));

    // On its own it leaves nothing behind.
    auto alone = ok(parser, {"--"});
    BOOST_CHECK(alone.values(0).empty());
}

// argparse asks this one about equals signs inside values.
BOOST_AUTO_TEST_CASE(test_only_the_first_equals_sign_splits) {
    Parser parser(Command("prog").addOption(Option({"-D", "--define"}, "Define").arg("expr")));

    BOOST_CHECK_EQUAL(ok(parser, {"--define=KEY=VALUE"}).valueForOption("-D"), "KEY=VALUE");
    BOOST_CHECK_EQUAL(ok(parser, {"--define="}).valueForOption("-D"), "");
    // A whole token with nothing before the sign is not an option anybody declared.
    bad(parser, {"=value"}, ParseResult::TooManyArguments);
}

// CLI11 has several tests about one option name being the start of another.
BOOST_AUTO_TEST_CASE(test_one_option_being_a_prefix_of_another) {
    Parser parser(Command("prog").addOptions({
        Option({"--out"}, "Out").arg("dir"),
        Option({"--output"}, "Output").arg("file"),
    }));

    // The whole token is tried before anything is taken apart, so the longer name is reachable.
    BOOST_CHECK_EQUAL(ok(parser, {"--output", "a"}).valueForOption("--output"), "a");
    BOOST_CHECK_EQUAL(ok(parser, {"--out", "b"}).valueForOption("--out"), "b");
    BOOST_CHECK(!ok(parser, {"--output", "a"}).isOptionSet("--out"));
}

// argtable3 tests the count of a repeatable flag rather than its values.
BOOST_AUTO_TEST_CASE(test_counting_a_flag_that_carries_nothing) {
    Parser parser(Command("prog").addOption(Option({"-v"}, "More talk").multi()));

    BOOST_CHECK_EQUAL(ok(parser, {}).option("-v").count(), 0);
    BOOST_CHECK_EQUAL(ok(parser, {"-v"}).option("-v").count(), 1);
    BOOST_CHECK_EQUAL(ok(parser, {"-v", "-v", "-v"}).option("-v").count(), 3);
    // Asking about one that was never declared is empty rather than a crash.
    BOOST_CHECK_EQUAL(ok(parser, {}).option("-q").count(), 0);
    BOOST_CHECK(ok(parser, {}).option("-q").option() == nullptr);
}

// CLI11's ExpectedRange cases, in the shape our arities give them.
BOOST_AUTO_TEST_CASE(test_how_few_and_how_many_a_multi_argument_takes) {
    Parser parser(Command("prog").addArgument(Argument("files").multi()));

    bad(parser, {}, ParseResult::MissingCommandArgument);
    BOOST_CHECK_EQUAL(ok(parser, {"one"}).values(0).size(), 1u);
    BOOST_CHECK_EQUAL(ok(parser, {"a", "b", "c", "d", "e"}).values(0).size(), 5u);

    // An optional one is content with nothing.
    Parser lenient(Command("prog").addArgument(Argument("files").multi().optional()));
    BOOST_CHECK(ok(lenient, {}).values(0).empty());
}

// Two multi arguments in a row, which is the case that shows the reservation rule is counting
// required arguments rather than arguments.
BOOST_AUTO_TEST_CASE(test_two_greedy_arguments_in_a_row) {
    Parser parser(Command("prog").addArguments({
        Argument("first").multi(),
        Argument("second").multi().optional(),
    }));

    // The first is greedy and the second is not required, so the first takes the lot.
    auto result = ok(parser, {"a", "b", "c"});
    BOOST_CHECK_EQUAL(result.values(0).size(), 3u);
    BOOST_CHECK(result.values(1).empty());
}

// Options and positionals interleaved, which every one of the three suites checks somewhere.
BOOST_AUTO_TEST_CASE(test_options_may_come_anywhere) {
    Parser parser(Command("prog")
                      .addArguments({Argument("a"), Argument("b")})
                      .addOption(Option({"-f"}, "Force"))
                      .addOption(Option({"-o"}, "Out").arg("dir")));

    for (auto args : {
             std::vector<std::string>{"-f",  "one", "two"},
             std::vector<std::string>{"one", "-f",  "two"},
             std::vector<std::string>{"one", "two", "-f" }
    }) {
        auto full = argv({});
        full.insert(full.end(), args.begin(), args.end());
        auto result = parser.parse(full);
        BOOST_REQUIRE_MESSAGE(result.isValid(), result.errorText());
        BOOST_CHECK(result.isOptionSet("-f"));
        BOOST_CHECK_EQUAL(result.value(0), "one");
        BOOST_CHECK_EQUAL(result.value(1), "two");
    }

    // An option's value is its own and is not counted as a positional.
    auto result = ok(parser, {"one", "-o", "dir", "two"});
    BOOST_CHECK_EQUAL(result.value(1), "two");
    BOOST_CHECK_EQUAL(result.valueForOption("-o"), "dir");
}

// A subcommand name that is also a value, which CLI11 tests as a fallthrough question.
BOOST_AUTO_TEST_CASE(test_a_subcommand_name_used_as_a_value) {
    Parser parser(Command("prog")
                      .addOption(Option({"-o"}, "Out").arg("dir"))
                      .addCommand(Command("copy").addArgument(Argument("src"))));

    // A command is only looked for where a command can be, which is before anything else.
    auto result = ok(parser, {"copy", "copy"});
    BOOST_CHECK_EQUAL(result.command()->name(), "copy");
    BOOST_CHECK_EQUAL(result.value(0), "copy");

    // An option in front of it does not stop the descent. The global is read against the root,
    // which declared it, and the subcommand is still reached.
    Parser global(Command("prog")
                      .addOption(Option({"-V"}, "Verbose").global())
                      .addCommand(Command("copy").addArgument(Argument("src"))));
    auto after = ok(global, {"-V", "copy", "x"});
    BOOST_CHECK_EQUAL(after.command()->name(), "copy");
    BOOST_CHECK(after.isOptionSet("-V"));
    BOOST_CHECK_EQUAL(after.value(0), "x");

    // Once a value has been taken, a name is a value and not a command any more. Without that
    // rule a program could never be given a file that happens to share a subcommand's name.
    Parser positional(
        Command("prog").addArguments({Argument("a"), Argument("b")}).addCommand(Command("copy")));
    auto late = ok(positional, {"x", "copy"});
    BOOST_CHECK_EQUAL(late.command()->name(), "prog");
    BOOST_CHECK_EQUAL(late.value(1), "copy");

    // Nor after a terminator, where nothing is a command.
    auto forced = ok(positional, {"--", "copy", "x"});
    BOOST_CHECK_EQUAL(forced.command()->name(), "prog");
    BOOST_CHECK_EQUAL(forced.value(0), "copy");
}

// Short matching carries exactly one value, so it is offered only to an option that wants
// exactly one and has to have it. Anything else would set half an option and leave the rest to
// be discovered as a missing argument somewhere further on.
BOOST_AUTO_TEST_CASE(test_short_matching_needs_one_required_argument) {
    Parser two(Command("prog").addOption(
        Option({"-o"}, "Two values").arg("a").arg("b").short_match(Option::ShortMatchAll)));
    bad(two, {"-oX"}, ParseResult::UnknownOption);
    // Written out it is fine.
    BOOST_CHECK(ok(two, {"-o", "X", "Y"}).isOptionSet("-o"));

    Parser optional(Command("prog").addOption(
        Option({"-p"}, "Maybe a value").arg("v", false).short_match(Option::ShortMatchAll)));
    bad(optional, {"-pX"}, ParseResult::UnknownOption);

    Parser none(Command("prog").addOption(
        Option({"-f"}, "No value at all").short_match(Option::ShortMatchAll)));
    bad(none, {"-fX"}, ParseResult::UnknownOption);
}

BOOST_AUTO_TEST_CASE(test_response_files) {
    auto path = std::filesystem::temp_directory_path() / "stdc_cli_response.txt";
    {
        std::ofstream file(path);
        file << "-f\n"
             << "one\n"
             << "\n"          // blank lines are nothing
             << "--out=dir\n" // and a joined value survives the trip
             << "two\n";
    }

    Parser parser(Command("prog")
                      .addArguments({Argument("a"), Argument("b")})
                      .addOption(Option({"-f"}, "Force"))
                      .addOption(Option({"--out"}, "Out").arg("dir")));

    auto result = ok(parser, {"@" + path.string()}, Parser::EnableResponseFile);
    BOOST_CHECK(result.isOptionSet("-f"));
    BOOST_CHECK_EQUAL(result.value(0), "one");
    BOOST_CHECK_EQUAL(result.value(1), "two");
    BOOST_CHECK_EQUAL(result.valueForOption("--out"), "dir");

    // Off by default, where the token is taken at face value instead of read as a file name.
    auto literal = ok(parser, {"@" + path.string(), "x"});
    BOOST_CHECK_EQUAL(literal.value(0), "@" + path.string());
    BOOST_CHECK_EQUAL(literal.value(1), "x");

    // A file that is not there is said so rather than passed along.
    bad(parser, {"@no_such_response_file.txt"}, ParseResult::ErrorReadingResponseFile,
        Parser::EnableResponseFile);

    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_CASE(test_an_empty_command_line_is_not_an_error_by_itself) {
    // Nothing declared, nothing given, nothing wrong. argparse tests this one because it is easy
    // to write a parser that trips over it.
    Parser parser(Command("prog"));
    BOOST_CHECK(parser.parse({}).isValid());
    BOOST_CHECK(parser.parse({"prog"}).isValid());
}

// ---------------------------------------------------------------------------------------------
// The help text
// ---------------------------------------------------------------------------------------------

namespace {

    /// Where \a needle starts, so that two of them can be compared for order.
    size_t at(const std::string &text, std::string_view needle) {
        auto pos = text.find(needle);
        BOOST_REQUIRE_MESSAGE(pos != std::string::npos, "\"" << needle << "\" is nowhere in:\n"
                                                             << text);
        return pos;
    }

    bool has(const std::string &text, std::string_view needle) {
        return text.find(needle) != std::string::npos;
    }

    /// The column the description starts at on the line holding \a left, for the cases about
    /// lining things up.
    size_t descriptionColumn(const std::string &text, std::string_view left) {
        auto start = text.rfind('\n', at(text, left)) + 1;
        auto line = text.substr(start, text.find('\n', start) - start);
        auto after = line.find(left) + left.size();
        auto column = line.find_first_not_of(' ', after);
        BOOST_REQUIRE_MESSAGE(column != std::string::npos, "nothing after \"" << left << "\"");
        return column;
    }

    Parser helpTree() {
        CommandCatalogue catalogue;
        catalogue.addCommands("Filesystem Commands", {"copy"})
            .addCommands("Buildsystem Commands", {"configure"});

        Parser parser(Command("prog", "What the program is for")
                          .addOptions({Option(Option::Help), Option(Option::Verbose).global()})
                          .addCommands({
                              Command("copy", "Copy things")
                                  .addArguments({Argument("src", "Where from").multi(),
                                                 Argument("dest", "Where to")})
                                  .addOption(Option({"-f", "--force"}, "Overwrite")),
                              Command("configure", "Configure things")
                                  .addArgument(Argument("mode", "Which way", false)
                                                   .default_value("fast")
                                                   .expect({"fast", "slow"}))
                                  .addOption(Option({"-p"}, "Project").arg("name").required()),
                              Command("orphan", "Not in any group"),
                          })
                          .setCatalogue(catalogue));
        parser.setPrologue("A prologue line");
        parser.setEpilogue("An epilogue line");
        return parser;
    }

}

BOOST_AUTO_TEST_CASE(test_help_layout_is_in_a_fixed_order) {
    auto text = helpTree().parse(argv({})).helpText();

    BOOST_CHECK(at(text, "A prologue line") < at(text, "What the program is for"));
    BOOST_CHECK(at(text, "What the program is for") < at(text, "Usage:"));
    BOOST_CHECK(at(text, "Usage:") < at(text, "Options:"));
    BOOST_CHECK(at(text, "Options:") < at(text, "Filesystem Commands:"));
    BOOST_CHECK(at(text, "Filesystem Commands:") < at(text, "An epilogue line"));
}

BOOST_AUTO_TEST_CASE(test_usage_names_the_path_it_took) {
    auto parser = helpTree();
    BOOST_CHECK(has(parser.parse(argv({})).helpText(), "Usage: prog [options] [commands]"));
    BOOST_CHECK(has(parser.parse(argv({"copy", "a", "b"})).helpText(),
                    "Usage: prog copy [options] <src>... <dest>"));
    // An optional argument is bracketed, and one that repeats carries the ellipsis.
    BOOST_CHECK(has(parser.parse(argv({"configure"})).helpText(),
                    "Usage: prog configure [options] [<mode>]"));
}

BOOST_AUTO_TEST_CASE(test_a_catalogue_names_the_headings_and_keeps_their_order) {
    auto text = helpTree().parse(argv({})).helpText();

    BOOST_CHECK(at(text, "Filesystem Commands:") < at(text, "Buildsystem Commands:"));
    // What the catalogue does not mention still shows, under the usual heading, at the end.
    // Anchored to the start of a line, since "Commands:" is a tail of the headings above it.
    BOOST_CHECK(at(text, "Buildsystem Commands:") < at(text, "\nCommands:"));
    BOOST_CHECK(at(text, "\nCommands:") < at(text, "orphan"));

    // Every command appears exactly once, wherever it was put.
    for (auto name : {"copy", "configure", "orphan"}) {
        BOOST_CHECK_MESSAGE(has(text, name), name);
    }
}

BOOST_AUTO_TEST_CASE(test_aligning_all_catalogues_shares_one_column) {
    auto parser = helpTree();

    parser.setDisplayOptions(Parser::Normal);
    auto apart = parser.parse(argv({})).helpText();
    // Group by group, a short group is narrow.
    BOOST_CHECK(descriptionColumn(apart, "copy") < descriptionColumn(apart, "-h, --help"));

    parser.setDisplayOptions(Parser::AlignAllCatalogues);
    auto together = parser.parse(argv({})).helpText();
    BOOST_CHECK_EQUAL(descriptionColumn(together, "copy"),
                      descriptionColumn(together, "configure"));
    BOOST_CHECK_EQUAL(descriptionColumn(together, "copy"),
                      descriptionColumn(together, "-h, --help"));
}

BOOST_AUTO_TEST_CASE(test_the_extras_are_asked_for) {
    auto parser = helpTree();

    auto plain = parser.parse(argv({"configure"})).helpText();
    BOOST_CHECK(!has(plain, "default:"));
    BOOST_CHECK(!has(plain, "fast, slow"));
    BOOST_CHECK(!has(plain, "(required)"));

    parser.setDisplayOptions(Parser::ShowArgumentDefaultValue | Parser::ShowArgumentExpectedValues |
                             Parser::ShowOptionIsRequired);
    auto full = parser.parse(argv({"configure"})).helpText();
    BOOST_CHECK(has(full, "(default: fast)"));
    BOOST_CHECK(has(full, "[fast, slow]"));
    BOOST_CHECK(has(full, "(required)"));
}

BOOST_AUTO_TEST_CASE(test_an_options_own_argument_carries_its_extras_too) {
    Parser parser(Command("prog").addOption(
        Option({"-l"}, "How loud")
            .arg(Argument("n", {}, false).default_value("1").expect({"0", "1", "2"}))));
    parser.setDisplayOptions(Parser::ShowArgumentDefaultValue | Parser::ShowArgumentExpectedValues);

    auto text = parser.parse(argv({})).helpText();
    BOOST_CHECK(has(text, "-l [<n>]"));
    BOOST_CHECK(has(text, "(default: 1)"));
    BOOST_CHECK(has(text, "[0, 1, 2]"));
}

BOOST_AUTO_TEST_CASE(test_roles_describe_themselves) {
    // The three options every program has are otherwise the three with nothing written beside
    // them, which is where a generated help text starts looking unfinished.
    Parser parser(Command("prog").addOptions({Option(Option::Help), Option(Option::Version),
                                              Option(Option::Verbose), Option(Option::Debug)}));
    auto text = parser.parse(argv({})).helpText();

    BOOST_CHECK(has(text, "Show this help and exit"));
    BOOST_CHECK(has(text, "Show the version and exit"));
    BOOST_CHECK(has(text, "Print more information"));
    BOOST_CHECK(has(text, "Print debugging information"));

    // A description of one's own wins.
    Parser named(Command("prog").addOption(Option(Option::Help, {}, "Read this")));
    BOOST_CHECK(has(named.parse(argv({})).helpText(), "Read this"));
    BOOST_CHECK(!has(named.parse(argv({})).helpText(), "Show this help"));
}

BOOST_AUTO_TEST_CASE(test_help_of_a_command_that_was_never_reached_is_empty) {
    // A default constructed result has no command, and asking it for help is empty rather than
    // a walk off the end.
    ParseResult empty;
    BOOST_CHECK(empty.helpText().empty());
    BOOST_CHECK(empty.command() == nullptr);
}

BOOST_AUTO_TEST_CASE(test_a_parser_can_be_built_and_returned) {
    // helpTree() above returns a named local, which needs the move that deleting the copy
    // suppressed. This is the case that found it.
    auto parser = helpTree();
    BOOST_CHECK_EQUAL(parser.rootCommand().name(), "prog");
    BOOST_CHECK_EQUAL(parser.prologue(), "A prologue line");

    Parser moved = std::move(parser);
    BOOST_CHECK_EQUAL(moved.rootCommand().name(), "prog");
    BOOST_CHECK(moved.parse(argv({"copy", "a", "b"})).isValid());
}

// ---------------------------------------------------------------------------------------------
// Shapes a whole program asks for
//
// Transcribing a real build tool's command tree against this library turned up three things
// nothing above had declared. They are here as what they are rather than as whose they were:
// which program wanted them is that program's business, and its own suite is where it belongs.
// ---------------------------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(test_an_option_carrying_two_arguments) {
    // One option, a pair of values, given more than once. Nothing above declares one, and a tool
    // that maps patterns onto directories is built out of little else.
    Parser parser(Command("prog")
                      .addArguments({Argument("src"), Argument("dest")})
                      .addOptions({Option({"-i", "--include"}, "A pattern and its subdirectory")
                                       .arg("regex")
                                       .arg("subdir")
                                       .multi(),
                                   Option({"-e", "--exclude"}, "A pattern").arg("regex").multi()}));

    auto result = ok(parser, {"src", "dst", "-i", "a", "x", "-i", "b", "y", "-e", "z"});
    BOOST_CHECK_EQUAL(result.value(0), "src");
    BOOST_CHECK_EQUAL(result.value(1), "dst");

    auto include = result.option("-i");
    BOOST_REQUIRE_EQUAL(include.count(), 2);
    // Slot by slot within one occurrence, which is the only way a pair means anything.
    BOOST_CHECK_EQUAL(include.rawValue(0, 0), "a");
    BOOST_CHECK_EQUAL(include.rawValue(1, 0), "x");
    BOOST_CHECK_EQUAL(include.rawValue(0, 1), "b");
    BOOST_CHECK_EQUAL(include.rawValue(1, 1), "y");
    // Or everything one slot ever took, across every occurrence.
    BOOST_CHECK(include.rawValues(0) == std::vector<std::string_view>({"a", "b"}));
    BOOST_CHECK(include.rawValues(1) == std::vector<std::string_view>({"x", "y"}));

    // Both of a pair are required, so half of one is a diagnostic.
    bad(parser, {"src", "dst", "-i", "a"}, ParseResult::MissingOptionArgument);

    // Unless the option says its own missing arguments are no matter.
    Parser lenient(Command("prog").addOption(Option({"-c"}, "A pair")
                                                 .arg("src")
                                                 .arg("dir")
                                                 .multi()
                                                 .prior(Option::IgnoreMissingArguments)));
    BOOST_CHECK(ok(lenient, {"-c"}).isOptionSet("-c"));
    BOOST_CHECK_EQUAL(ok(lenient, {"-c", "a", "b"}).option("-c").rawValue(1), "b");
}

BOOST_AUTO_TEST_CASE(test_the_two_options_every_program_has) {
    // Written out by hand these need the right prior level to work at all, which is a thing to
    // know rather than a thing to guess.
    Parser parser(Command("prog")
                      .addArgument(Argument("required one"))
                      .addVersionOption("1.2.3")
                      .addHelpOption(true, true)
                      .addCommand(Command("copy").addArgument(Argument("src"))));

    // A bare program name prints help rather than complaining about what is missing.
    auto bare = ok(parser, {});
    BOOST_CHECK(bare.isRoleSet(Option::Help));

    // Asked for on a line that is missing everything, it is still answered.
    BOOST_CHECK(ok(parser, {"--help"}).isRoleSet(Option::Help));
    BOOST_CHECK(ok(parser, {"--version"}).isRoleSet(Option::Version));

    // Global, so the subcommands have it too.
    BOOST_CHECK(ok(parser, {"copy", "--help"}).isRoleSet(Option::Help));

    // The version is kept on the command, which is what the option is there to print.
    BOOST_CHECK_EQUAL(parser.rootCommand().version(), "1.2.3");

    // Spellings and descriptions can still be the caller's.
    Parser renamed(Command("prog").addHelpOption(false, false, {"-?"}, "How to use this"));
    BOOST_CHECK(ok(renamed, {"-?"}).isRoleSet(Option::Help));
    BOOST_CHECK(has(renamed.parse(argv({})).helpText(), "How to use this"));
}

BOOST_AUTO_TEST_CASE(test_a_tree_of_several_commands_reads_as_one_page) {
    CommandCatalogue catalogue;
    catalogue.addCommands("Filesystem Commands", {"copy", "rmdir", "touch"})
        .addCommands("Buildsystem Commands", {"configure", "incsync", "deploy"});

    Command root("tool", "Utility commands");
    for (auto [name, desc] : {
             std::pair{"copy",      "Copy files"          },
             {"rmdir",     "Remove directories"  },
             {"touch",     "Update timestamps"   },
             {"configure", "Generate a header"   },
             {"incsync",   "Reorganize headers"  },
             {"deploy",    "Resolve dependencies"}
    }) {
        root.addCommand(Command(name, desc).addArgument(Argument("path").multi()));
    }
    root.addVersionOption("1.0").addHelpOption(true, true).setCatalogue(catalogue);

    Parser parser(std::move(root));
    parser.setDisplayOptions(Parser::AlignAllCatalogues);
    auto text = parser.parse(argv({})).helpText();

    BOOST_CHECK(at(text, "Filesystem Commands:") < at(text, "Buildsystem Commands:"));
    for (auto name : {"copy", "rmdir", "touch", "configure", "incsync", "deploy"}) {
        BOOST_CHECK_MESSAGE(has(text, name), name);
    }
    // Six commands over two headings still line up as one table.
    BOOST_CHECK_EQUAL(descriptionColumn(text, "copy"), descriptionColumn(text, "configure"));

    // And the tree still parses, which a help text alone would not say.
    BOOST_CHECK_EQUAL(ok(parser, {"deploy", "a", "b"}).command()->name(), "deploy");
}

BOOST_AUTO_TEST_SUITE_END()
