// SPDX-License-Identifier: MIT

/// \file commandline.h
///
/// Declaring what a program takes on its command line, and reading back what it was given.
///
/// The shape of the API comes from SysCmdLine, https://github.com/SineStriker/syscmdline, which
/// this replaces. A program moving across is renaming rather than rewriting. Where the two part
/// on what a command line means, this says so beside Parser.

#ifndef STDCORELIB_COMMANDLINE_H
#define STDCORELIB_COMMANDLINE_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <stdcorelib/stdc_global.h>
#include <stdcorelib/adt/array_view.h>

namespace stdc::cli {

    /// How a token is turned into a \c T, and what to call \c T in the help text.
    ///
    /// A command line is text, so everything here is stored as text and converted when it is
    /// read. Specialize this to accept a type of your own:
    ///
    /// \code
    ///   template <>
    ///   struct stdc::cli::value_traits<fs::path> {
    ///       static bool parse(std::string_view token, fs::path *out) {
    ///           *out = token;
    ///           return true;
    ///       }
    ///       static const char *type_name() {
    ///           return "path";
    ///       }
    ///   };
    /// \endcode
    ///
    /// \c parse returns false for a token the type cannot represent, which is what turns
    /// \c --count=x into a diagnostic rather than a zero.
    template <class T, class Enable = void>
    struct value_traits;

    namespace detail {

        /// A type's check and its name, as function pointers, so that Argument can hold a
        /// type without being a template.
        struct value_type_info {
            /// Whether the token is a \c T. Null means anything goes, which is the default.
            bool (*check)(std::string_view) = nullptr;
            /// The name used in diagnostics and in the help text. Must be a literal, since
            /// it is held rather than copied.
            const char *name = nullptr;
        };

        template <class T>
        bool check_value(std::string_view token) {
            T out{};
            return value_traits<T>::parse(token, &out);
        }

        template <class T>
        value_type_info type_info_for() {
            return {&check_value<T>, value_traits<T>::type_name()};
        }

        /// What a ParseResult holds.
        class parse_data;

        STDC_EXPORT bool parse_signed(std::string_view token, int64_t *out, int64_t min,
                                      int64_t max);
        STDC_EXPORT bool parse_unsigned(std::string_view token, uint64_t *out, uint64_t max);
        STDC_EXPORT bool parse_floating(std::string_view token, double *out);
        STDC_EXPORT bool parse_boolean(std::string_view token, bool *out);

    }

    /// Text, which is what a command line already is.
    template <>
    struct value_traits<std::string> {
        static inline bool parse(std::string_view token, std::string *out) {
            out->assign(token);
            return true;
        }
        static inline const char *type_name() {
            return "string";
        }
    };

    /// A view into the result's own storage, which outlives the read.
    template <>
    struct value_traits<std::string_view> {
        static inline bool parse(std::string_view token, std::string_view *out) {
            *out = token;
            return true;
        }
        static inline const char *type_name() {
            return "string";
        }
    };

    /// \c true, \c false, \c yes, \c no, \c on, \c off, \c 1 and \c 0, in any case.
    template <>
    struct value_traits<bool> {
        static inline bool parse(std::string_view token, bool *out) {
            return detail::parse_boolean(token, out);
        }
        static inline const char *type_name() {
            return "bool";
        }
    };

    /// Every integer type except \c bool, which has its own above. The range of the target
    /// type is part of the check, so \c 300 is not a \c uint8_t.
    template <class T>
    struct value_traits<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
        static inline bool parse(std::string_view token, T *out) {
            if constexpr (std::is_signed_v<T>) {
                int64_t v;
                if (!detail::parse_signed(token, &v, int64_t((std::numeric_limits<T>::min)()),
                                          int64_t((std::numeric_limits<T>::max)()))) {
                    return false;
                }
                *out = T(v);
            } else {
                uint64_t v;
                if (!detail::parse_unsigned(token, &v, uint64_t((std::numeric_limits<T>::max)()))) {
                    return false;
                }
                *out = T(v);
            }
            return true;
        }
        static inline const char *type_name() {
            return std::is_signed_v<T> ? "int" : "uint";
        }
    };

    /// \c float, \c double and \c long double. Uses \c strtod rather than \c from_chars, which
    /// libc++ did not implement for floating point for a long time.
    template <class T>
    struct value_traits<T, std::enable_if_t<std::is_floating_point_v<T>>> {
        static inline bool parse(std::string_view token, T *out) {
            double v;
            if (!detail::parse_floating(token, &v)) {
                return false;
            }
            *out = T(v);
            return true;
        }
        static inline const char *type_name() {
            return "number";
        }
    };

    class ParseResult;

    /// One positional value a command or an option takes.
    class Argument {
    public:
        /// How many tokens it takes.
        enum Arity {
            /// Exactly one.
            Single,
            /// One or more. Leaves enough tokens for the required arguments after it.
            Multiple,
            /// Everything left, including anything that looks like an option.
            Remainder,
        };

        /// Answers whether \a token is acceptable, and says why in \a error when it is not.
        using Validator = std::function<bool(std::string_view token, std::string *error)>;

        Argument() = default;

        inline Argument(std::string name, std::string desc = {}, bool required = true)
            : _name(std::move(name)), _desc(std::move(desc)), _required(required) {
        }

        /// The name to show in the help text, when it should differ from name().
        inline Argument &metavar(std::string displayName) {
            _displayName = std::move(displayName);
            return *this;
        }
        inline Argument &required(bool on = true) {
            _required = on;
            return *this;
        }
        inline Argument &optional(bool on = true) {
            _required = !on;
            return *this;
        }
        /// The value the result gives when the argument was not given. Stored as text and
        /// converted when read.
        ///
        /// \pre It is readable as whatever type<T>() declared, and is one of the values
        ///      expect() allows, if either was given.
        inline Argument &defaultValue(std::string value) {
            _default = std::move(value);
            _hasDefault = true;
            assertDefaultIsUsable();
            return *this;
        }
        /// The only values this accepts, for an argument that is a choice between a few
        /// words.
        ///
        /// \pre Every one of them is readable as whatever type<T>() declared.
        inline Argument &expect(std::vector<std::string> values) {
            _expected = std::move(values);
            assertExpectedMatchType();
            assertDefaultIsUsable();
            return *this;
        }
        inline Argument &validate(Validator validator) {
            _validator = std::move(validator);
            return *this;
        }
        inline Argument &nargs(Arity arity) {
            _arity = arity;
            return *this;
        }
        inline Argument &multi(bool on = true) {
            _arity = on ? Multiple : Single;
            return *this;
        }
        /// Declares the type. Tokens are checked against it while parsing, and its name
        /// appears in the help text. Without it any token is accepted.
        ///
        /// \pre Whatever expect() was given, if anything, is readable as a \c T.
        template <class T>
        inline Argument &type() {
            _type = detail::type_info_for<T>();
            assertExpectedMatchType();
            assertDefaultIsUsable();
            return *this;
        }

        inline const std::string &name() const {
            return _name;
        }
        inline const std::string &description() const {
            return _desc;
        }
        /// The metavar if one was given, and the name otherwise.
        inline const std::string &displayName() const {
            return _displayName.empty() ? _name : _displayName;
        }
        inline bool isRequired() const {
            return _required;
        }
        inline bool hasDefaultValue() const {
            return _hasDefault;
        }
        inline const std::string &defaultValue() const {
            return _default;
        }
        inline const std::vector<std::string> &expectedValues() const {
            return _expected;
        }
        inline const Validator &validator() const {
            return _validator;
        }
        inline Arity arity() const {
            return _arity;
        }
        inline const detail::value_type_info &typeInfo() const {
            return _type;
        }

    private:
        /// Any of the three may be written first, so each calls what it can now check.
        inline void assertExpectedMatchType() const {
            if (!_type.check) {
                return;
            }
            for (const auto &item : _expected) {
                assert(_type.check(item) &&
                       "an expected value of this argument is not of the type it declared");
                (void) item;
            }
        }
        inline void assertDefaultIsUsable() const {
            if (!_hasDefault) {
                return;
            }
            assert((!_type.check || _type.check(_default)) &&
                   "the default value of this argument is not of the type it declared");
            assert((_expected.empty() ||
                    std::find(_expected.begin(), _expected.end(), _default) != _expected.end()) &&
                   "the default value of this argument is not one of the values it expects");
        }

        std::string _name;
        std::string _desc;
        std::string _displayName;
        std::string _default;
        std::vector<std::string> _expected;
        Validator _validator;
        detail::value_type_info _type;
        Arity _arity = Single;
        bool _required = true;
        bool _hasDefault = false;
    };

    /// A named switch, with any number of arguments of its own.
    class Option {
    public:
        /// What the option means, for the few every program has. A role brings the usual
        /// spellings, and lets a caller ask by role rather than by spelling.
        enum Role {
            NoRole,
            Debug,
            Verbose,
            Version,
            Help,
        };

        /// How much of a short token the parser may take for this option, so that \c -O2 or
        /// \c -DKEY=VALUE can be one token rather than two.
        enum ShortMatch {
            /// \c -D and its value are separate tokens.
            NoShortMatch,
            /// A single letter may be followed by the value, as in \c -O2.
            ShortMatchSingleLetter,
            /// A single character, letter or not.
            ShortMatchSingleChar,
            /// The whole token after the option's own, as in \c -DKEY=VALUE.
            ShortMatchAll,
        };

        /// The highest level among the options given decides. This is what lets \c --help be
        /// answered on a command line that is missing everything it requires.
        enum Prior {
            NoPrior,
            /// Its own missing arguments are not an error.
            IgnoreMissingArguments,
            /// Nothing missing anywhere is an error.
            IgnoreMissingSymbols,
            /// Set it when nothing else was given at all.
            AutoSetWhenNoSymbols,
            /// Giving it means no arguments may be given.
            ExclusiveToArguments,
            /// Giving it means no other options may be given.
            ExclusiveToOptions,
            /// Giving it means nothing else may be given.
            ExclusiveToAll,
        };

        Option() = default;

        inline Option(std::vector<std::string> tokens, std::string desc = {})
            : _tokens(std::move(tokens)), _desc(std::move(desc)) {
        }
        inline Option(std::initializer_list<std::string> tokens, std::string desc = {})
            : Option(std::vector<std::string>(tokens), std::move(desc)) {
        }
        inline Option(std::string token, std::string desc = {})
            : Option(std::vector<std::string>{std::move(token)}, std::move(desc)) {
        }
        /// Deliberately not explicit, so that \c addOptions({Option::Verbose}) reads the way it
        /// does. Empty tokens take the usual spelling for the role.
        inline Option(Role role, std::vector<std::string> tokens = {}, std::string desc = {})
            : _tokens(tokens.empty() ? defaultTokens(role) : std::move(tokens)),
              _desc(desc.empty() ? defaultDescription(role) : std::move(desc)), _role(role) {
        }

        /// Adds an argument. An option's argument needs no description of its own.
        inline Option &arg(std::string name, bool required = true) {
            _args.emplace_back(Argument(std::move(name), {}, required));
            return *this;
        }
        inline Option &arg(Argument argument) {
            _args.emplace_back(std::move(argument));
            return *this;
        }
        inline Option &required(bool on = true) {
            _required = on;
            return *this;
        }
        inline Option &shortMatch(ShortMatch rule) {
            _shortMatch = rule;
            return *this;
        }
        inline Option &prior(Prior level) {
            _prior = level;
            return *this;
        }
        /// Visible to this command's subcommands as well as to itself.
        inline Option &global(bool on = true) {
            _global = on;
            return *this;
        }
        /// How many times it may be given, zero meaning without limit. The last argument added is
        /// the one that repeats.
        inline Option &multi(int maxOccurrence = 0) {
            _maxOccurrence = maxOccurrence;
            return *this;
        }

        inline const std::vector<std::string> &tokens() const {
            return _tokens;
        }
        /// The first spelling, which is the one the help text and diagnostics use.
        inline const std::string &token() const {
            return _tokens.front();
        }
        inline const std::string &description() const {
            return _desc;
        }
        inline const std::vector<Argument> &arguments() const {
            return _args;
        }
        inline bool isRequired() const {
            return _required;
        }
        inline bool isGlobal() const {
            return _global;
        }
        inline Role role() const {
            return _role;
        }
        inline ShortMatch shortMatch() const {
            return _shortMatch;
        }
        inline Prior prior() const {
            return _prior;
        }
        inline int maxOccurrence() const {
            return _maxOccurrence;
        }

        /// What a role says about itself in the help text when nothing else was given.
        static inline std::string defaultDescription(Role role) {
            switch (role) {
                case Help:
                    return "Show this help and exit";
                case Version:
                    return "Show the version and exit";
                case Verbose:
                    return "Print more information";
                case Debug:
                    return "Print debugging information";
                default:
                    return {};
            }
        }

        /// The spellings a role answers to when none were given.
        static inline std::vector<std::string> defaultTokens(Role role) {
            switch (role) {
                case Help:
                    return {"-h", "--help"};
                case Version:
                    return {"-v", "--version"};
                case Verbose:
                    return {"-V", "--verbose"};
                case Debug:
                    return {"-d", "--debug"};
                default:
                    return {};
            }
        }

    private:
        std::vector<std::string> _tokens;
        std::string _desc;
        std::vector<Argument> _args;
        Role _role = NoRole;
        ShortMatch _shortMatch = NoShortMatch;
        Prior _prior = NoPrior;
        int _maxOccurrence = 1;
        bool _required = false;
        bool _global = false;
    };

    /// Which heading each name is listed under in the help text. Anything not named here goes
    /// under the default heading, so a catalogue only has to mention what it wants to move.
    class CommandCatalogue {
    public:
        struct Group {
            std::string name;
            std::vector<std::string> members;
        };

        inline CommandCatalogue &addCommands(std::string group, std::vector<std::string> names) {
            _commands.push_back({std::move(group), std::move(names)});
            return *this;
        }
        inline CommandCatalogue &addOptions(std::string group, std::vector<std::string> names) {
            _options.push_back({std::move(group), std::move(names)});
            return *this;
        }
        inline CommandCatalogue &addArguments(std::string group, std::vector<std::string> names) {
            _arguments.push_back({std::move(group), std::move(names)});
            return *this;
        }

        inline const std::vector<Group> &commandGroups() const {
            return _commands;
        }
        inline const std::vector<Group> &optionGroups() const {
            return _options;
        }
        inline const std::vector<Group> &argumentGroups() const {
            return _arguments;
        }
        inline bool isEmpty() const {
            return _commands.empty() && _options.empty() && _arguments.empty();
        }

    private:
        std::vector<Group> _commands;
        std::vector<Group> _options;
        std::vector<Group> _arguments;
    };

    /// A command, its arguments, its options and whatever subcommands it has.
    class Command {
    public:
        /// What to run once this command is the one that was named. Its return value is the
        /// program's.
        using Handler = std::function<int(const ParseResult &)>;

        Command() = default;

        inline Command(std::string name, std::string desc = {})
            : _name(std::move(name)), _desc(std::move(desc)) {
        }

        inline Command &addArgument(Argument argument) {
            _args.emplace_back(std::move(argument));
            return *this;
        }
        inline Command &addArguments(std::vector<Argument> arguments) {
            for (auto &item : arguments) {
                _args.emplace_back(std::move(item));
            }
            return *this;
        }
        inline Command &addOption(Option option) {
            _options.emplace_back(std::move(option));
            return *this;
        }
        inline Command &addOptions(std::vector<Option> options) {
            for (auto &item : options) {
                _options.emplace_back(std::move(item));
            }
            return *this;
        }
        inline Command &addCommand(Command command) {
            _commands.emplace_back(std::move(command));
            return *this;
        }
        inline Command &addCommands(std::vector<Command> commands) {
            for (auto &item : commands) {
                _commands.emplace_back(std::move(item));
            }
            return *this;
        }
        inline Command &setHandler(Handler handler) {
            _handler = std::move(handler);
            return *this;
        }
        inline Command &setCatalogue(CommandCatalogue catalogue) {
            _catalogue = std::move(catalogue);
            return *this;
        }
        /// What a Version option prints.
        inline Command &setVersion(std::string version) {
            _version = std::move(version);
            return *this;
        }

        /// The version option, with the level that lets it be answered on a command line that is
        /// otherwise missing everything it needs.
        inline Command &addVersionOption(std::string version, std::vector<std::string> tokens = {},
                                         std::string desc = {}) {
            _version = std::move(version);
            return addOption(Option(Option::Version, std::move(tokens), std::move(desc))
                                 .prior(Option::IgnoreMissingSymbols));
        }

        /// The help option, likewise.
        ///
        /// \param showIfNoArguments Answer a command line with nothing on it at all, so that a
        ///        bare program name prints its help.
        /// \param global Keep it in scope for the subcommands as well.
        /// \param tokens The spellings, or the usual ones when empty.
        /// \param desc The description, or the usual one when empty.
        inline Command &addHelpOption(bool showIfNoArguments = false, bool global = false,
                                      std::vector<std::string> tokens = {}, std::string desc = {}) {
            return addOption(Option(Option::Help, std::move(tokens), std::move(desc))
                                 .prior(showIfNoArguments ? Option::AutoSetWhenNoSymbols
                                                          : Option::IgnoreMissingSymbols)
                                 .global(global));
        }
        inline Command &setDescription(std::string desc) {
            _desc = std::move(desc);
            return *this;
        }

        inline const std::string &name() const {
            return _name;
        }
        inline const std::string &description() const {
            return _desc;
        }
        inline const std::string &version() const {
            return _version;
        }
        inline const std::vector<Argument> &arguments() const {
            return _args;
        }
        inline const std::vector<Option> &options() const {
            return _options;
        }
        inline const std::vector<Command> &commands() const {
            return _commands;
        }
        inline const Handler &handler() const {
            return _handler;
        }
        inline const CommandCatalogue &catalogue() const {
            return _catalogue;
        }

        /// The subcommand named \a name, or null. Only one level down.
        inline const Command *findCommand(std::string_view name) const {
            for (const auto &item : _commands) {
                if (item._name == name) {
                    return &item;
                }
            }
            return nullptr;
        }

        /// The option answering to \a token, or null. A token is any of an option's spellings,
        /// not only the first.
        inline const Option *findOption(std::string_view token) const {
            for (const auto &item : _options) {
                for (const auto &spelling : item.tokens()) {
                    if (spelling == token) {
                        return &item;
                    }
                }
            }
            return nullptr;
        }

    private:
        std::string _name;
        std::string _desc;
        std::string _version;
        std::vector<Argument> _args;
        std::vector<Option> _options;
        std::vector<Command> _commands;
        Handler _handler;
        CommandCatalogue _catalogue;
    };

    /// What one option was given.
    ///
    /// A view onto the ParseResult it came from, the way \c std::string_view is a view onto a
    /// string. It owns nothing and keeps nothing alive.
    ///
    /// \warning Do not outlive that result. \c parser.parse(args).option("-f") reads freed
    ///          storage at the semicolon, since the result it was taken from was a temporary.
    /// \sa ParseResult::option()
    class STDC_EXPORT OptionResult {
    public:
        /// How many times the option was given.
        int count() const;
        inline bool isSet() const {
            return count() > 0;
        }
        /// The option itself, or null when it was never declared.
        const Option *option() const;

        /// The \a index'th argument of the \a occurrence'th appearance, as text, or the
        /// default value where there is one. Empty when there is neither.
        ///
        /// \warning Points into the ParseResult and lasts exactly as long as it does. Ask
        ///          value<T>() for something that owns what it holds.
        std::string_view rawValue(int index = 0, int occurrence = 0) const;

        /// Every value the \a index'th argument took, across every occurrence.
        ///
        /// \warning The same. These point into the ParseResult.
        std::vector<std::string_view> rawValues(int index = 0) const;

        /// Converted, or nothing when there is nothing to convert.
        ///
        /// Nothing means one of two things: no token is there and no default value stands in
        /// for it, or a token is there that is not a \c T. Declaring the type on the Argument
        /// turns the second into a diagnostic while parsing, which leaves this meaning only the
        /// first.
        ///
        /// \note An option given an empty value, \c --prefix= , reads as nothing here too,
        ///       since what comes back is the same empty text either way. isSet() tells those
        ///       two apart, and value<T>() gives the empty string rather than nothing.
        ///
        /// \code
        ///   int jobs = result.option("-j").tryValue<int>().value_or(default_jobs());
        /// \endcode
        template <class T = std::string>
        std::optional<T> tryValue(int index = 0, int occurrence = 0) const {
            auto raw = rawValue(index, occurrence);
            T out{};
            if (raw.empty() || !value_traits<T>::parse(raw, &out)) {
                return std::nullopt;
            }
            return out;
        }

        /// The same without the check. A token that is not a \c T gives a value initialized
        /// \c T. Declare the type on the Argument to have that rejected while parsing instead.
        template <class T = std::string>
        T value(int index = 0, int occurrence = 0) const {
            T out{};
            value_traits<T>::parse(rawValue(index, occurrence), &out);
            return out;
        }

        template <class T = std::string>
        std::vector<T> values(int index = 0) const {
            std::vector<T> out;
            for (auto raw : rawValues(index)) {
                T item{};
                value_traits<T>::parse(raw, &item);
                out.push_back(std::move(item));
            }
            return out;
        }

    private:
        friend class ParseResult;
        inline OptionResult(const void *data) : _data(data) {
        }
        const void *_data;
    };

    /// What a command line turned out to mean, or why it did not.
    class STDC_EXPORT ParseResult {
    public:
        enum Error {
            NoError,
            UnknownOption,
            UnknownCommand,
            MissingOptionArgument,
            MissingCommandArgument,
            TooManyArguments,
            InvalidArgumentValue,
            MissingRequiredOption,
            OptionOccurTooMuch,
            ArgumentTypeMismatch,
            ArgumentValidateFailed,
            PriorOptionWithArguments,
            PriorOptionWithOptions,
            ErrorReadingResponseFile,
        };

        ParseResult();
        ParseResult(const ParseResult &other);
        ParseResult(ParseResult &&other) noexcept;
        ParseResult &operator=(const ParseResult &other);
        ParseResult &operator=(ParseResult &&other) noexcept;
        ~ParseResult();

        inline bool isValid() const {
            return error() == NoError;
        }
        Error error() const;
        /// What went wrong, ready to be printed.
        const std::string &errorText() const;
        /// The declared names close enough to what was typed to be worth offering, ready to be
        /// printed. Empty when nothing is close, and when the failure was not a mistyped name.
        std::string correctionText() const;

        /// The command that was reached, which is the root when no subcommand was named.
        const Command *command() const;
        /// The names from the root down to it, the root first.
        const std::vector<std::string> &commandPath() const;

        /// Runs the handler of the command that was reached. \a errorCode is returned instead
        /// when the parse failed or the command has no handler.
        int invoke(int errorCode = -1) const;

        bool isOptionSet(std::string_view token) const;
        /// Whether an option carrying \a role was given, whatever it was spelled as.
        bool isRoleSet(Option::Role role) const;
        OptionResult option(std::string_view token) const;

        /// The \a index'th positional argument of the command that was reached, as text.
        ///
        /// \warning Points into this result and lasts exactly as long as it does. Ask value<T>()
        ///          for something that owns what it holds.
        std::string_view rawValue(int index) const;
        /// Every token the \a index'th positional argument took.
        ///
        /// \warning The same. These point into this result.
        std::vector<std::string_view> rawValues(int index) const;

        /// Converted, or nothing when there is nothing to convert.
        ///
        /// Nothing means one of two things: no token is there and no default value stands in
        /// for it, or a token is there that is not a \c T. Declaring the type on the Argument
        /// turns the second into a diagnostic while parsing, which leaves this meaning only the
        /// first.
        ///
        /// \note An option given an empty value, \c --prefix= , reads as nothing here too,
        ///       since what comes back is the same empty text either way. isSet() tells those
        ///       two apart, and value<T>() gives the empty string rather than nothing.
        ///
        /// \code
        ///   int jobs = result.tryValue<int>(0).value_or(default_jobs());
        /// \endcode
        template <class T = std::string>
        std::optional<T> tryValue(int index) const {
            auto raw = rawValue(index);
            T out{};
            if (raw.empty() || !value_traits<T>::parse(raw, &out)) {
                return std::nullopt;
            }
            return out;
        }

        template <class T = std::string>
        T value(int index) const {
            T out{};
            value_traits<T>::parse(rawValue(index), &out);
            return out;
        }
        template <class T = std::string>
        std::vector<T> values(int index) const {
            std::vector<T> out;
            for (auto raw : rawValues(index)) {
                T item{};
                value_traits<T>::parse(raw, &item);
                out.push_back(std::move(item));
            }
            return out;
        }

        /// The first argument of \a token's first occurrence.
        template <class T = std::string>
        T valueForOption(std::string_view token) const {
            return option(token).value<T>();
        }

        // @overload: valueForOption, saying whether there was one
        template <class T = std::string>
        std::optional<T> tryValueForOption(std::string_view token) const {
            return option(token).tryValue<T>();
        }

        /// The help text for the command that was reached, prologue and epilogue included.
        std::string helpText() const;
        /// Writes helpText() to stdout.
        void showHelp() const;
        /// Writes what went wrong to stderr, with a line saying how to ask for help. Does
        /// nothing when the parse succeeded.
        void showError() const;

    private:
        friend class Parser;
        std::shared_ptr<detail::parse_data> _impl;
    };

    /// Turns arguments into a ParseResult against a command tree.
    ///
    /// \li A subcommand is looked for after the options the root declared, so
    ///     \c prog \c -V \c copy \c x reaches \c copy. An option belonging to the subcommand
    ///     rather than to the root is unknown in front of it.
    /// \li Positional tokens a command cannot take are an error.
    /// \li An option that needs a value will not take a token that is a declared option of the
    ///     same command. Anything else beginning with a dash is a value as it is there.
    class STDC_EXPORT Parser {
    public:
        /// What the tokenizer will accept beyond the usual.
        enum ParseOption {
            Standard = 0,
            /// Subcommand names match without regard to case.
            IgnoreCommandCase = 0x1,
            /// Option tokens match without regard to case.
            IgnoreOptionCase = 0x2,
            /// \c -abc means \c -a \c -b \c -c.
            AllowUnixGroupFlags = 0x4,
            /// \c /f is another way of writing \c -f.
            AllowDosShortOptions = 0x8,
            /// A single dash starts nothing.
            DontAllowUnixShortOptions = 0x10,
            /// \c \@file is replaced by the lines of that file.
            EnableResponseFile = 0x20,
        };

        /// What the help text says beyond the necessary. The layout is fixed: prologue,
        /// description, usage, arguments, options, commands, epilogue.
        enum DisplayOption {
            Normal = 0,
            /// Say what an argument falls back to when it is not given.
            ShowArgumentDefaultValue = 0x1,
            /// List the words an argument accepts, where it accepts only a few.
            ShowArgumentExpectedValues = 0x2,
            /// Mark the options that have to be given.
            ShowOptionIsRequired = 0x4,
            /// Line the descriptions of every group up with each other, so that a catalogue
            /// reads as one table.
            AlignAllCatalogues = 0x8,
            /// Keep showError() from offering the names close to what was typed.
            SkipCorrection = 0x10,
        };

        Parser();
        explicit Parser(Command root);
        ~Parser();

        Parser(const Parser &other) = delete;
        Parser &operator=(const Parser &other) = delete;

        /// Movable, so that a parser can be built and returned by a function of its own.
        Parser(Parser &&other) noexcept;
        Parser &operator=(Parser &&other) noexcept;

        /// Sets a new root command, replacing the one given to the constructor.
        ///
        /// \note Do not change it once parse() has been called. A ParseResult reads the tree it
        ///       was parsed against, and keeps reading the old one.
        void setRootCommand(Command root);
        const Command &rootCommand() const;

        /// Printed above and below the help text.
        void setPrologue(std::string text);
        const std::string &prologue() const;
        void setEpilogue(std::string text);
        const std::string &epilogue() const;

        /// A bitwise or of DisplayOption values.
        void setDisplayOptions(int options);
        int displayOptions() const;

        /// How many columns the help text may use, which is what its descriptions are wrapped
        /// to.
        ///
        /// \param width the column count, or 0 to ask the terminal each time the text is made
        /// \note 0 is the default. Where there is no terminal to ask, as when the output is a
        ///       pipe, that comes out as 80 columns, so a program's help reads the same however
        ///       it is captured.
        /// \sa console::width()
        void setTextWidth(int width);
        int textWidth() const;

        ParseResult parse(const std::vector<std::string> &args, int parseOptions = Standard) const;
        /// Parses and runs the handler that was reached, which is what a \c main wants.
        inline int invoke(const std::vector<std::string> &args, int errorCode = -1,
                          int parseOptions = Standard) const {
            return parse(args, parseOptions).invoke(errorCode);
        }

        /// The same, taking what \c main was handed.
        ///
        /// \warning Not on Windows. What \c main is given there is in the system code page,
        ///          while everything here is UTF-8, so a non-ASCII argument arrives wrong.
        ///          system::command_line_arguments() gives the same list already converted, on
        ///          every platform.
        inline ParseResult parse(int argc, char **argv, int parseOptions = Standard) const {
            return parse(std::vector<std::string>(argv, argv + argc), parseOptions);
        }
        inline int invoke(int argc, char **argv, int errorCode = -1,
                          int parseOptions = Standard) const {
            return parse(argc, argv, parseOptions).invoke(errorCode);
        }

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // STDCORELIB_COMMANDLINE_H
