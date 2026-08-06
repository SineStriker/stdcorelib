// SPDX-License-Identifier: MIT

#ifndef STDCORELIB_COMMANDLINE_H
#define STDCORELIB_COMMANDLINE_H

#include <cstdint>
#include <functional>
#include <limits>
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

        /// The two halves of a type, kept as function pointers so that Argument can carry a type
        /// without being a template and without a virtual call per token.
        struct value_type_info {
            /// Whether the token is a \c T. Null means anything goes, which is the default.
            bool (*check)(std::string_view) = nullptr;
            /// What to call it when a diagnostic or the help text has to name it.
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

    /// Every integer type but \c bool, which is spelled out above. The range of the target type is
    /// part of the check, so \c 300 is not a \c uint8_t.
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

    // Argument, Option, Command and CommandCatalogue are plain values, defined here rather than
    // exported. They are built once at startup out of literals, so there is nothing for a pimpl
    // to hide and nothing for reference counting to save, and an exported class holding a
    // std::vector is what C4251 is about.

    /// One positional value a command or an option takes.
    class Argument {
    public:
        /// How many tokens it eats.
        enum Arity {
            /// Exactly one.
            Single,
            /// One or more, stopping early enough to leave what follows its own tokens.
            Multiple,
            /// Everything that is left, options included and unexamined.
            Remainder,
        };

        /// Answers whether \a token is acceptable, and says why in \a error when it is not.
        using Validator = std::function<bool(std::string_view token, std::string *error)>;

        Argument() = default;

        inline Argument(std::string name, std::string desc = {}, bool required = true)
            : _name(std::move(name)), _desc(std::move(desc)), _required(required) {
        }

        /// What to call it in the help text, when the name is not what a reader wants to see.
        inline Argument &metavar(std::string display_name) {
            _display_name = std::move(display_name);
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
        /// Handed back by the result when the argument was not given. Stored as text, like
        /// everything else, and converted by whoever reads it.
        inline Argument &default_value(std::string value) {
            _default = std::move(value);
            _has_default = true;
            return *this;
        }
        /// The complete set of tokens this will accept, for the arguments that are really a
        /// choice between a few words.
        inline Argument &expect(std::vector<std::string> values) {
            _expected = std::move(values);
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
        /// Declares what the tokens mean, which is both a check made while parsing and a word for
        /// the help text. Without it anything is acceptable and the type shows up as a string.
        template <class T>
        inline Argument &type() {
            _type = detail::type_info_for<T>();
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
            return _display_name.empty() ? _name : _display_name;
        }
        inline bool isRequired() const {
            return _required;
        }
        inline bool hasDefaultValue() const {
            return _has_default;
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
        std::string _name;
        std::string _desc;
        std::string _display_name;
        std::string _default;
        std::vector<std::string> _expected;
        Validator _validator;
        detail::value_type_info _type;
        Arity _arity = Single;
        bool _required = true;
        bool _has_default = false;
    };

    /// A named switch, with however many arguments of its own.
    class Option {
    public:
        /// What the program means by the option, for the few that every program has. A role picks
        /// the usual tokens and lets the parser act on it without being told the spelling.
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

        /// A ladder, compared rather than switched on: the highest level among the options given
        /// is the one that decides. This is what lets \c --help be answered on a command line
        /// that is otherwise missing everything it requires.
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
              _desc(std::move(desc)), _role(role) {
        }

        /// An argument of its own. An option's argument carries no description, having the
        /// option's to stand on.
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
        inline Option &short_match(ShortMatch rule = ShortMatchAll) {
            _short_match = rule;
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
        inline Option &multi(int max_occurrence = 0) {
            _max_occurrence = max_occurrence;
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
            return _short_match;
        }
        inline Prior prior() const {
            return _prior;
        }
        inline int maxOccurrence() const {
            return _max_occurrence;
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
        ShortMatch _short_match = NoShortMatch;
        Prior _prior = NoPrior;
        int _max_occurrence = 1;
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
        /// What a Version option prints. Setting it is what makes that option worth having.
        inline Command &setVersion(std::string version) {
            _version = std::move(version);
            return *this;
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

}

#endif // STDCORELIB_COMMANDLINE_H
