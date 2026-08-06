// SPDX-License-Identifier: MIT

#include "commandline.h"

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <unordered_map>

namespace stdc::cli {

    namespace detail {

        namespace {

            /// Whether \a token is entirely made of what \c from_chars consumed. A number that
            /// stops short of the end of the token, \c 12abc, is not a number.
            bool consumed_all(std::string_view token, const char *end) {
                return end == token.data() + token.size();
            }

            /// \c from_chars refuses a leading plus, which a command line will hand over anyway.
            std::string_view drop_leading_plus(std::string_view token) {
                if (token.size() > 1 && token.front() == '+') {
                    token.remove_prefix(1);
                }
                return token;
            }

            char lowered(char c) {
                return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
            }

            /// Both sides are folded, not just the first. Folding one is enough for a comparison
            /// against a literal that is already lower case, and wrong for anything else.
            bool equals_ignoring_case(std::string_view token, std::string_view other) {
                if (token.size() != other.size()) {
                    return false;
                }
                for (size_t i = 0; i < token.size(); ++i) {
                    if (lowered(token[i]) != lowered(other[i])) {
                        return false;
                    }
                }
                return true;
            }

        }

        bool parse_signed(std::string_view token, int64_t *out, int64_t min, int64_t max) {
            token = drop_leading_plus(token);
            if (token.empty()) {
                return false;
            }
            int64_t v = 0;
            auto res = std::from_chars(token.data(), token.data() + token.size(), v);
            if (res.ec != std::errc{} || !consumed_all(token, res.ptr)) {
                return false;
            }
            if (v < min || v > max) {
                return false;
            }
            *out = v;
            return true;
        }

        bool parse_unsigned(std::string_view token, uint64_t *out, uint64_t max) {
            token = drop_leading_plus(token);
            if (token.empty()) {
                return false;
            }
            // No sign to refuse by hand: from_chars into an unsigned rejects a minus outright.
            // Checked on all three of MSVC, libstdc++ and libc++, each answering invalid_argument
            // and leaving the output alone. The test says so too, since it is their promise this
            // relies on rather than ours.
            uint64_t v = 0;
            auto res = std::from_chars(token.data(), token.data() + token.size(), v);
            if (res.ec != std::errc{} || !consumed_all(token, res.ptr)) {
                return false;
            }
            if (v > max) {
                return false;
            }
            *out = v;
            return true;
        }

        bool parse_floating(std::string_view token, double *out) {
            if (token.empty()) {
                return false;
            }
            // strtod rather than from_chars: libc++ went years without the floating point
            // overload, and macOS is one of the platforms this has to work on.
            std::string buf(token);
            errno = 0;
            char *end = nullptr;
            double v = std::strtod(buf.c_str(), &end);
            if (end != buf.c_str() + buf.size() || end == buf.c_str()) {
                return false;
            }
            if (errno == ERANGE) {
                return false;
            }
            *out = v;
            return true;
        }

        bool parse_boolean(std::string_view token, bool *out) {
            static const std::string_view yes[] = {"true", "yes", "on", "1"};
            static const std::string_view no[] = {"false", "no", "off", "0"};
            for (auto word : yes) {
                if (equals_ignoring_case(token, word)) {
                    *out = true;
                    return true;
                }
            }
            for (auto word : no) {
                if (equals_ignoring_case(token, word)) {
                    *out = false;
                    return true;
                }
            }
            return false;
        }

    }

    // ---------------------------------------------------------------------------------------
    // Storage
    // ---------------------------------------------------------------------------------------

    namespace {

        /// The tokens one declared argument took. More than one only where the argument said it
        /// accepts more than one.
        using ArgumentValues = std::vector<std::string>;

        /// One appearance of an option, holding a slot per argument it declares.
        using Occurrence = std::vector<ArgumentValues>;

        struct OptionData {
            const Option *option = nullptr;
            std::vector<Occurrence> occurrences;
        };

        bool same_token(std::string_view a, std::string_view b, bool ignore_case) {
            if (!ignore_case) {
                return a == b;
            }
            return detail::equals_ignoring_case(a, b);
        }

    }

    class detail::parse_data {
    public:
        /// Shared with the parser rather than copied, so that the pointers below stay good and
        /// the tree is walked once.
        std::shared_ptr<const Command> root;
        const Command *target = nullptr;
        std::vector<std::string> path;

        /// Per positional argument of the command that was reached.
        std::vector<ArgumentValues> arguments;
        /// Every option in scope, its own and whatever was inherited.
        std::vector<OptionData> options;
        std::unordered_map<std::string, size_t> by_token;

        ParseResult::Error error = ParseResult::NoError;
        std::string error_text;

        const OptionData *find(std::string_view token) const {
            auto it = by_token.find(std::string(token));
            return it == by_token.end() ? nullptr : &options[it->second];
        }
    };

    // ---------------------------------------------------------------------------------------
    // OptionResult
    // ---------------------------------------------------------------------------------------

    int OptionResult::count() const {
        auto data = static_cast<const OptionData *>(_data);
        return data ? int(data->occurrences.size()) : 0;
    }

    const Option *OptionResult::option() const {
        auto data = static_cast<const OptionData *>(_data);
        return data ? data->option : nullptr;
    }

    std::string_view OptionResult::rawValue(int index, int occurrence) const {
        auto data = static_cast<const OptionData *>(_data);
        if (!data || occurrence < 0 || size_t(occurrence) >= data->occurrences.size()) {
            return {};
        }
        const auto &slots = data->occurrences[size_t(occurrence)];
        if (index < 0 || size_t(index) >= slots.size() || slots[size_t(index)].empty()) {
            return {};
        }
        return slots[size_t(index)].front();
    }

    std::vector<std::string_view> OptionResult::rawValues(int index) const {
        std::vector<std::string_view> res;
        auto data = static_cast<const OptionData *>(_data);
        if (!data || index < 0) {
            return res;
        }
        for (const auto &slots : data->occurrences) {
            if (size_t(index) >= slots.size()) {
                continue;
            }
            for (const auto &item : slots[size_t(index)]) {
                res.emplace_back(item);
            }
        }
        return res;
    }

    // ---------------------------------------------------------------------------------------
    // ParseResult
    // ---------------------------------------------------------------------------------------

    ParseResult::ParseResult() : _impl(std::make_shared<detail::parse_data>()) {
    }

    ParseResult::ParseResult(const ParseResult &other) = default;
    ParseResult::ParseResult(ParseResult &&other) noexcept = default;
    ParseResult &ParseResult::operator=(const ParseResult &other) = default;
    ParseResult &ParseResult::operator=(ParseResult &&other) noexcept = default;
    ParseResult::~ParseResult() = default;

    ParseResult::Error ParseResult::error() const {
        return _impl->error;
    }

    const std::string &ParseResult::errorText() const {
        return _impl->error_text;
    }

    const Command *ParseResult::command() const {
        return _impl->target;
    }

    const std::vector<std::string> &ParseResult::commandPath() const {
        return _impl->path;
    }

    int ParseResult::invoke(int errorCode) const {
        if (!isValid() || !_impl->target || !_impl->target->handler()) {
            return errorCode;
        }
        return _impl->target->handler()(*this);
    }

    bool ParseResult::isOptionSet(std::string_view token) const {
        auto data = _impl->find(token);
        return data && !data->occurrences.empty();
    }

    bool ParseResult::isRoleSet(Option::Role role) const {
        if (role == Option::NoRole) {
            return false;
        }
        for (const auto &item : _impl->options) {
            if (item.option->role() == role && !item.occurrences.empty()) {
                return true;
            }
        }
        return false;
    }

    OptionResult ParseResult::option(std::string_view token) const {
        return OptionResult(_impl->find(token));
    }

    std::string_view ParseResult::rawValue(int index) const {
        if (index < 0 || size_t(index) >= _impl->arguments.size() ||
            _impl->arguments[size_t(index)].empty()) {
            return {};
        }
        return _impl->arguments[size_t(index)].front();
    }

    std::vector<std::string_view> ParseResult::rawValues(int index) const {
        std::vector<std::string_view> res;
        if (index < 0 || size_t(index) >= _impl->arguments.size()) {
            return res;
        }
        for (const auto &item : _impl->arguments[size_t(index)]) {
            res.emplace_back(item);
        }
        return res;
    }

    // ---------------------------------------------------------------------------------------
    // Parsing
    // ---------------------------------------------------------------------------------------

    namespace {

        /// Everything one call to parse needs, kept together so the steps can hand off to each
        /// other without a dozen parameters.
        class ParserCore {
        public:
            ParserCore(detail::parse_data *out, int flags) : r(out), flags(flags) {
            }

            void run(const std::vector<std::string> &args);

        private:
            using Error = ParseResult::Error;

            detail::parse_data *r;
            int flags;
            std::vector<std::string> tokens;
            size_t pos = 0;
            /// The positional tokens, gathered first and handed to the arguments afterwards,
            /// since how many each takes depends on how many there are.
            std::vector<std::string> positional;
            /// The highest priority option given, which is what decides whether the checks at
            /// the end are worth making.
            const Option *prior_option = nullptr;
            /// The globals of every command walked through, which stay in scope below.
            std::vector<const Option *> inherited;
            bool saw_terminator = false;

            bool on(int flag) const {
                return (flags & flag) != 0;
            }
            bool failed() const {
                return r->error != ParseResult::NoError;
            }
            void fail(Error error, std::string text) {
                if (!failed()) {
                    r->error = error;
                    r->error_text = std::move(text);
                }
            }

            void expandResponseFiles();
            const Command *subcommandFor(const std::string &token) const;
            void enter(const Command *next);
            void collectOptions();
            void readTokens();
            bool readOption(const std::string &token);
            bool readOneOption(const OptionData *data, std::string_view inline_value);
            const OptionData *lookup(std::string_view token) const;
            bool readGroupedFlags(const std::string &token);
            void assignPositional();
            void applyDefaults();
            void checkRequired();

            bool looksLikeOption(std::string_view token) const;
            bool accepts(const Argument &argument, const std::string &token,
                         const std::string &where);
            void notePrior(const Option *option);
        };

        bool ParserCore::looksLikeOption(std::string_view token) const {
            if (saw_terminator || token.size() < 2) {
                return false;
            }
            if (token[0] == '-') {
                return token[1] == '-' || !on(Parser::DontAllowUnixShortOptions);
            }
            return token[0] == '/' && on(Parser::AllowDosShortOptions);
        }

        void ParserCore::notePrior(const Option *option) {
            if (!prior_option || option->prior() > prior_option->prior()) {
                if (option->prior() != Option::NoPrior) {
                    prior_option = option;
                }
            }
        }

        void ParserCore::expandResponseFiles() {
            std::vector<std::string> out;
            for (const auto &token : tokens) {
                if (token.size() < 2 || token.front() != '@') {
                    out.push_back(token);
                    continue;
                }
                std::ifstream file(token.substr(1));
                if (!file) {
                    fail(ParseResult::ErrorReadingResponseFile,
                         "cannot read response file \"" + token.substr(1) + "\"");
                    return;
                }
                std::string line;
                while (std::getline(file, line)) {
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }
                    if (!line.empty()) {
                        out.push_back(line);
                    }
                }
            }
            tokens = std::move(out);
        }

        /// The subcommand \a token names, or null. Only worth asking before the first positional
        /// token, since after that a name is a value.
        const Command *ParserCore::subcommandFor(const std::string &token) const {
            if (!positional.empty() || saw_terminator) {
                return nullptr;
            }
            for (const auto &candidate : r->target->commands()) {
                if (same_token(candidate.name(), token, on(Parser::IgnoreCommandCase))) {
                    return &candidate;
                }
            }
            return nullptr;
        }

        /// Moves into \a next, keeping whatever the command being left declared global.
        void ParserCore::enter(const Command *next) {
            for (const auto &option : r->target->options()) {
                if (option.isGlobal()) {
                    inherited.push_back(&option);
                }
            }
            r->target = next;
            r->path.push_back(next->name());
            collectOptions();
        }

        /// What can be written from here: this command's own options, plus the globals of every
        /// command above it. An option of a command already left behind is not matched again,
        /// though whatever it collected on the way stays readable.
        void ParserCore::collectOptions() {
            r->by_token.clear();
            auto add = [this](const Option *option) {
                size_t index = r->options.size();
                for (size_t i = 0; i < r->options.size(); ++i) {
                    if (r->options[i].option == option) {
                        index = i;
                        break;
                    }
                }
                if (index == r->options.size()) {
                    r->options.push_back({option, {}});
                }
                for (const auto &spelling : option->tokens()) {
                    r->by_token[spelling] = index;
                }
            };
            for (const auto &option : r->target->options()) {
                add(&option);
            }
            for (auto option : inherited) {
                add(option);
            }
        }

        const OptionData *ParserCore::lookup(std::string_view token) const {
            if (!on(Parser::IgnoreOptionCase)) {
                return r->find(token);
            }
            for (const auto &item : r->options) {
                for (const auto &spelling : item.option->tokens()) {
                    if (detail::equals_ignoring_case(spelling, token)) {
                        return &item;
                    }
                }
            }
            return nullptr;
        }

        bool ParserCore::accepts(const Argument &argument, const std::string &token,
                                 const std::string &where) {
            const auto &expected = argument.expectedValues();
            if (!expected.empty() &&
                std::find(expected.begin(), expected.end(), token) == expected.end()) {
                fail(ParseResult::InvalidArgumentValue,
                     "\"" + token + "\" is not one of the values " + where + " accepts");
                return false;
            }
            if (argument.typeInfo().check && !argument.typeInfo().check(token)) {
                fail(ParseResult::ArgumentTypeMismatch, "\"" + token + "\" is not a " +
                                                            argument.typeInfo().name +
                                                            ", which is what " + where + " takes");
                return false;
            }
            if (argument.validator()) {
                std::string reason;
                if (!argument.validator()(token, &reason)) {
                    fail(ParseResult::ArgumentValidateFailed,
                         reason.empty() ? "\"" + token + "\" is not acceptable to " + where
                                        : reason);
                    return false;
                }
            }
            return true;
        }

        /// Reads the arguments of one option occurrence, starting at \c pos. \a inline_value is
        /// what came after an equals sign or was joined to a short token, and stands in for the
        /// first argument when there is one.
        bool ParserCore::readOneOption(const OptionData *data, std::string_view inline_value) {
            auto mutable_data = const_cast<OptionData *>(data);
            const Option *option = data->option;

            int limit = option->maxOccurrence();
            if (limit > 0 && int(data->occurrences.size()) >= limit) {
                fail(ParseResult::OptionOccurTooMuch,
                     "option \"" + option->token() + "\" was given more than " +
                         std::to_string(limit) + (limit == 1 ? " time" : " times"));
                return false;
            }

            Occurrence slots(option->arguments().size());
            bool have_inline = inline_value.data() != nullptr;

            for (size_t i = 0; i < option->arguments().size(); ++i) {
                const auto &argument = option->arguments()[i];
                const std::string where = "\"" + option->token() + "\"";

                if (i == 0 && have_inline) {
                    std::string token(inline_value);
                    if (!accepts(argument, token, where)) {
                        return false;
                    }
                    slots[i].push_back(std::move(token));
                    continue;
                }

                bool multiple = argument.arity() != Argument::Single;
                bool took_any = false;
                while (pos < tokens.size()) {
                    const auto &token = tokens[pos];
                    // A token that is somebody's option is never quietly eaten as a value, not
                    // even by an argument that has to have one. Saying "-o needs a value" is
                    // worth more than taking --force and leaving the reader to work out where
                    // it went. Writing -o=--force or putting -- first is how it is forced.
                    //
                    // Only a declared option counts, so a negative number is a value like any
                    // other rather than an option nobody has heard of.
                    if (looksLikeOption(token) && lookup(token) != nullptr) {
                        break;
                    }
                    if (!accepts(argument, token, where)) {
                        return false;
                    }
                    slots[i].push_back(token);
                    ++pos;
                    took_any = true;
                    if (!multiple) {
                        break;
                    }
                }

                if (!took_any && argument.isRequired() &&
                    option->prior() < Option::IgnoreMissingArguments) {
                    fail(ParseResult::MissingOptionArgument, "option \"" + option->token() +
                                                                 "\" needs a value for <" +
                                                                 argument.displayName() + ">");
                    return false;
                }
            }

            mutable_data->occurrences.push_back(std::move(slots));
            notePrior(option);
            return true;
        }

        bool ParserCore::readGroupedFlags(const std::string &token) {
            if (!on(Parser::AllowUnixGroupFlags) || token.size() < 3 || token[0] != '-' ||
                token[1] == '-') {
                return false;
            }
            // Every letter has to be an option of its own that wants no value, or the whole
            // token is something else and is left alone.
            std::vector<const OptionData *> found;
            for (size_t i = 1; i < token.size(); ++i) {
                auto data = lookup(std::string("-") + token[i]);
                if (!data || !data->option->arguments().empty()) {
                    return false;
                }
                found.push_back(data);
            }
            for (auto data : found) {
                if (!readOneOption(data, {})) {
                    return false;
                }
            }
            return true;
        }

        bool ParserCore::readOption(const std::string &token) {
            // The whole token, which is the ordinary case.
            if (auto data = lookup(token)) {
                return readOneOption(data, {});
            }

            // A value joined by an equals sign.
            auto equals = token.find('=');
            if (equals != std::string::npos) {
                if (auto data = lookup(token.substr(0, equals))) {
                    if (data->option->arguments().empty()) {
                        fail(ParseResult::UnknownOption,
                             "option \"" + token.substr(0, equals) + "\" takes no value");
                        return false;
                    }
                    return readOneOption(data, std::string_view(token).substr(equals + 1));
                }
            }

            // A short option with its value stuck to it, which only an option with exactly one
            // required argument can be.
            for (const auto &item : r->options) {
                const Option *option = item.option;
                if (option->shortMatch() == Option::NoShortMatch ||
                    option->arguments().size() != 1 || !option->arguments().front().isRequired()) {
                    continue;
                }
                for (const auto &spelling : option->tokens()) {
                    if (spelling.size() >= token.size() ||
                        token.compare(0, spelling.size(), spelling) != 0) {
                        continue;
                    }
                    if (option->shortMatch() != Option::ShortMatchAll && spelling.size() != 2) {
                        continue;
                    }
                    if (option->shortMatch() == Option::ShortMatchSingleLetter &&
                        !std::isalpha(static_cast<unsigned char>(spelling[1]))) {
                        continue;
                    }
                    return readOneOption(&item, std::string_view(token).substr(spelling.size()));
                }
            }

            if (readGroupedFlags(token)) {
                return true;
            }

            // A DOS token spelled the other way round.
            if (token[0] == '/' && on(Parser::AllowDosShortOptions)) {
                if (auto data = lookup("-" + token.substr(1))) {
                    return readOneOption(data, {});
                }
            }

            fail(ParseResult::UnknownOption, "unknown option \"" + token + "\"");
            return false;
        }

        void ParserCore::readTokens() {
            while (pos < tokens.size() && !failed()) {
                const auto &token = tokens[pos];
                if (!saw_terminator && token == "--") {
                    saw_terminator = true;
                    ++pos;
                    continue;
                }
                if (looksLikeOption(token)) {
                    ++pos;
                    readOption(token);
                    continue;
                }
                // A subcommand is looked for here rather than in a pass of its own, so that the
                // global options a command line puts in front of one are read against the
                // command that declared them.
                if (auto next = subcommandFor(token)) {
                    ++pos;
                    enter(next);
                    continue;
                }
                positional.push_back(token);
                ++pos;
            }
        }

        void ParserCore::assignPositional() {
            const auto &declared = r->target->arguments();
            r->arguments.resize(declared.size());

            size_t taken = 0;
            for (size_t i = 0; i < declared.size() && taken < positional.size(); ++i) {
                const auto &argument = declared[i];
                size_t take = 1;
                if (argument.arity() == Argument::Remainder) {
                    take = positional.size() - taken;
                } else if (argument.arity() == Argument::Multiple) {
                    // Leave one token for each required argument still to come, or a greedy
                    // multi would eat the destination of a copy.
                    size_t reserved = 0;
                    for (size_t j = i + 1; j < declared.size(); ++j) {
                        if (declared[j].isRequired()) {
                            ++reserved;
                        }
                    }
                    size_t available = positional.size() - taken;
                    take = available > reserved ? available - reserved : 1;
                }
                take = (std::min) (take, positional.size() - taken);

                const std::string where = "<" + argument.displayName() + ">";
                for (size_t k = 0; k < take; ++k) {
                    const auto &token = positional[taken + k];
                    if (!accepts(argument, token, where)) {
                        return;
                    }
                    r->arguments[i].push_back(token);
                }
                taken += take;
            }

            if (taken < positional.size()) {
                fail(ParseResult::TooManyArguments, "\"" + positional[taken] +
                                                        "\" is one argument more than \"" +
                                                        r->target->name() + "\" takes");
            }
        }

        void ParserCore::applyDefaults() {
            const auto &declared = r->target->arguments();
            for (size_t i = 0; i < declared.size() && i < r->arguments.size(); ++i) {
                if (r->arguments[i].empty() && declared[i].hasDefaultValue()) {
                    r->arguments[i].push_back(declared[i].defaultValue());
                }
            }
            for (auto &item : r->options) {
                for (auto &occurrence : item.occurrences) {
                    for (size_t i = 0; i < occurrence.size(); ++i) {
                        if (occurrence[i].empty() &&
                            item.option->arguments()[i].hasDefaultValue()) {
                            occurrence[i].push_back(item.option->arguments()[i].defaultValue());
                        }
                    }
                }
            }
        }

        void ParserCore::checkRequired() {
            // An option high enough on the ladder answers the command line by itself, so what is
            // missing elsewhere is no longer a complaint worth making.
            auto level = prior_option ? prior_option->prior() : Option::NoPrior;

            // The three exclusive levels each forbid their own thing rather than everything
            // below them. Only the ladder's lower half is a ladder.
            bool forbids_arguments =
                level == Option::ExclusiveToArguments || level == Option::ExclusiveToAll;
            bool forbids_options =
                level == Option::ExclusiveToOptions || level == Option::ExclusiveToAll;

            if (forbids_arguments) {
                bool any_argument = false;
                for (const auto &values : r->arguments) {
                    any_argument = any_argument || !values.empty();
                }
                if (any_argument) {
                    fail(ParseResult::PriorOptionWithArguments,
                         "option \"" + prior_option->token() + "\" takes no arguments beside it");
                    return;
                }
            }
            if (forbids_options) {
                for (const auto &item : r->options) {
                    if (item.option != prior_option && !item.occurrences.empty()) {
                        fail(ParseResult::PriorOptionWithOptions,
                             "option \"" + prior_option->token() + "\" takes no options beside it");
                        return;
                    }
                }
            }
            if (level >= Option::IgnoreMissingSymbols) {
                return;
            }

            const auto &declared = r->target->arguments();
            for (size_t i = 0; i < declared.size(); ++i) {
                if (declared[i].isRequired() && r->arguments[i].empty()) {
                    fail(ParseResult::MissingCommandArgument, "\"" + r->target->name() +
                                                                  "\" needs a value for <" +
                                                                  declared[i].displayName() + ">");
                    return;
                }
            }
            for (const auto &item : r->options) {
                if (item.option->isRequired() && item.occurrences.empty()) {
                    fail(ParseResult::MissingRequiredOption,
                         "option \"" + item.option->token() + "\" is required");
                    return;
                }
            }
        }

        void ParserCore::run(const std::vector<std::string> &args) {
            // The first argument names the program rather than anything to parse.
            tokens.assign(args.begin() + (args.empty() ? 0 : 1), args.end());
            if (on(Parser::EnableResponseFile)) {
                expandResponseFiles();
                if (failed()) {
                    return;
                }
            }

            r->path.push_back(r->target->name());
            collectOptions();
            readTokens();
            if (failed()) {
                return;
            }

            // An option that stands in for an empty command line, which is what makes a bare
            // command print its help rather than complain.
            if (!prior_option && tokens.empty()) {
                for (const auto &item : r->options) {
                    if (item.option->prior() == Option::AutoSetWhenNoSymbols) {
                        const_cast<OptionData &>(item).occurrences.emplace_back(
                            item.option->arguments().size());
                        prior_option = item.option;
                        break;
                    }
                }
            }

            assignPositional();
            if (failed()) {
                return;
            }
            applyDefaults();
            checkRequired();
        }

    }

    // ---------------------------------------------------------------------------------------
    // Parser
    // ---------------------------------------------------------------------------------------

    class Parser::Impl {
    public:
        std::shared_ptr<Command> root = std::make_shared<Command>();
        std::string prologue;
        std::string epilogue;
    };

    Parser::Parser() : _impl(std::make_unique<Impl>()) {
    }

    Parser::Parser(Command root) : _impl(std::make_unique<Impl>()) {
        *_impl->root = std::move(root);
    }

    Parser::~Parser() = default;

    void Parser::setRootCommand(Command root) {
        *_impl->root = std::move(root);
    }

    const Command &Parser::rootCommand() const {
        return *_impl->root;
    }

    void Parser::setPrologue(std::string text) {
        _impl->prologue = std::move(text);
    }

    const std::string &Parser::prologue() const {
        return _impl->prologue;
    }

    void Parser::setEpilogue(std::string text) {
        _impl->epilogue = std::move(text);
    }

    const std::string &Parser::epilogue() const {
        return _impl->epilogue;
    }

    ParseResult Parser::parse(const std::vector<std::string> &args, int parseOptions) const {
        ParseResult result;
        result._impl->root = _impl->root;
        result._impl->target = _impl->root.get();
        ParserCore(result._impl.get(), parseOptions).run(args);
        return result;
    }

    int Parser::invoke(const std::vector<std::string> &args, int errorCode,
                       int parseOptions) const {
        return parse(args, parseOptions).invoke(errorCode);
    }

}
