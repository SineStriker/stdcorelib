// SPDX-License-Identifier: MIT

#include "commandline.h"

#include <cerrno>
#include <cstdio>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <unordered_map>

#include "console.h"
#include "utf.h"

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

        /// Copied from the parser, so that a result can print its own help without being handed
        /// the parser that made it.
        std::string prologue;
        std::string epilogue;
        int display_options = Parser::Normal;
        int text_width = 0;

        ParseResult::Error error = ParseResult::NoError;
        std::string error_text;

        /// What was typed where a declared name belongs, and the names it might have meant.
        /// Kept rather than measured here, so a program that never prints a correction does not
        /// pay for one.
        std::string error_token;
        std::vector<std::string> error_candidates;

        const OptionData *find(std::string_view token) const {
            auto it = by_token.find(std::string(token));
            return it == by_token.end() ? nullptr : &options[it->second];
        }

        /// The same, for the parser, which fills these in. Writing it out beats a const_cast
        /// admitting the const above was never true of this data.
        OptionData *findForWriting(std::string_view token) {
            auto it = by_token.find(std::string(token));
            return it == by_token.end() ? nullptr : &options[it->second];
        }
    };

    namespace {

        /// Defined further down, beside the rest of the layout. Named here because the result's
        /// accessors come first and one of them prints.
        std::string helpFor(const Command &command, const std::vector<std::string> &path,
                            const std::vector<const Option *> &inherited,
                            const std::string &prologue, const std::string &epilogue, int flags,
                            int text_width);

    }

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

    namespace {

        /// How many single character insertions, deletions and substitutions it takes to turn
        /// one into the other. Two rows rather than the whole table, since only the previous one
        /// is ever read.
        size_t edit_distance(const std::string &a, const std::string &b) {
            std::vector<size_t> row(b.size() + 1);
            for (size_t j = 0; j <= b.size(); ++j) {
                row[j] = j;
            }
            for (size_t i = 1; i <= a.size(); ++i) {
                size_t diagonal = row[0];
                row[0] = i;
                for (size_t j = 1; j <= b.size(); ++j) {
                    size_t above = row[j];
                    row[j] = (std::min)({row[j] + 1, row[j - 1] + 1,
                                         diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
                    diagonal = above;
                }
            }
            return row[b.size()];
        }

    }

    std::string ParseResult::correctionText() const {
        const auto &input = _impl->error_token;
        if (input.empty() || _impl->error_candidates.empty()) {
            return {};
        }

        // Half of what was typed. Looser than that and every short name is a candidate for every
        // short typo, which is worse than saying nothing.
        const size_t threshold = input.size() / 2;

        std::string suggestions;
        for (const auto &item : _impl->error_candidates) {
            if (edit_distance(input, item) <= threshold) {
                suggestions += "\n  " + item;
            }
        }
        if (suggestions.empty()) {
            return {};
        }
        return "\"" + input + "\" is not matched. Do you mean one of the following?" + suggestions;
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

    std::string ParseResult::helpText() const {
        if (!_impl->target) {
            return {};
        }
        // Zero means ask, and the answer is whatever stdout is: a terminal's width, or 80
        // columns for a pipe or a file, so help captured into one reads the same everywhere.
        int width = _impl->text_width > 0 ? _impl->text_width : console::width(stdout);

        // The globals of every command above this one, gathered by walking down the path the
        // way the parser did. What it gathered on the way is what it demands here, so this is
        // where the help text has to agree with it.
        std::vector<const Option *> inherited;
        if (_impl->root && _impl->path.size() > 1) {
            const Command *at = _impl->root.get();
            for (size_t i = 1; i < _impl->path.size() && at; ++i) {
                for (const auto &option : at->options()) {
                    if (option.isGlobal()) {
                        inherited.push_back(&option);
                    }
                }
                at = at->findCommand(_impl->path[i]);
            }
        }

        return helpFor(*_impl->target, _impl->path, inherited, _impl->prologue, _impl->epilogue,
                       _impl->display_options, width);
    }

    void ParseResult::showHelp() const {
        // Through the library's own console rather than fwrite, so that one program does not
        // talk to the terminal two different ways, and so a Windows console gets the transcoding
        // it needs.
        console::fputs(console::nostyle, console::nocolor, console::nocolor, helpText(), stdout);
    }

    void ParseResult::showError() const {
        if (isValid()) {
            return;
        }
        // What went wrong is worth a color where there is one to be had, and console works out
        // for itself whether stderr is somewhere escapes belong.
        console::fputs(console::bold, console::red, console::nocolor, _impl->error_text + "\n",
                       stderr);
        if (!(_impl->display_options & Parser::SkipCorrection)) {
            auto correction = correctionText();
            if (!correction.empty()) {
                console::fputs(console::nostyle, console::nocolor, console::nocolor,
                               correction + "\n", stderr);
            }
        }
        if (_impl->target) {
            std::string name;
            for (size_t i = 0; i < _impl->path.size(); ++i) {
                name += (i ? " " : "") + _impl->path[i];
            }
            console::fputs(console::nostyle, console::nocolor, console::nocolor,
                           "Try \"" + name + " --help\" for more information.\n", stderr);
        }
    }

    // ---------------------------------------------------------------------------------------
    // Help
    // ---------------------------------------------------------------------------------------

    namespace {

        /// One line of a list: what it is called on the left, what it does on the right.
        struct Entry {
            std::string left;
            std::string right;
        };

        /// A heading and the lines under it. A list with nothing to group has one group whose
        /// name is the usual heading.
        struct Section {
            std::string title;
            std::vector<Entry> entries;
        };

        constexpr int indent = 2;
        constexpr int gap = 4;

        /// However narrow the terminal, a description gets at least this much. Below it the text
        /// is broken into a column too thin to read, which is worse than running over.
        constexpr int min_description = 20;

        /// Breaks \a text into lines of at most \a columns columns.
        ///
        /// At spaces where there are any, and between characters where there are none, which is
        /// what a language that writes without spaces needs. Newlines already in \a text are
        /// kept, so a caller who laid out their own paragraphs keeps them.
        ///
        /// Measured in columns rather than in bytes or characters, so a CJK description breaks
        /// where it looks like it should. \sa console::display_width()
        std::vector<std::string> wrapped(const std::string &text, int columns) {
            std::vector<std::string> lines;
            if (columns < 1) {
                lines.push_back(text);
                return lines;
            }

            auto points = utf::utf8_to_utf32(text);
            std::u32string line;
            int width = 0;
            const auto emit = [&lines](std::u32string piece) {
                while (!piece.empty() && piece.back() == U' ') {
                    piece.pop_back();
                }
                lines.push_back(utf::utf32_to_utf8(piece));
            };
            const auto measure = [](const std::u32string &piece) {
                int res = 0;
                for (char32_t c : piece) {
                    res += console::display_width(c);
                }
                return res;
            };

            for (char32_t c : points) {
                if (c == U'\n') {
                    emit(std::move(line));
                    line.clear();
                    width = 0;
                    continue;
                }
                int w = console::display_width(c);
                if (width + w > columns && !line.empty()) {
                    // Back up to the last space, so a word is not cut in half. A word longer
                    // than the whole column has no space to back up to and is broken where it
                    // reached the edge.
                    auto space = line.find_last_of(U' ');
                    if (space == std::u32string::npos) {
                        emit(line);
                        line.clear();
                    } else {
                        auto tail = line.substr(space + 1);
                        emit(line.substr(0, space));
                        line = tail;
                    }
                    width = measure(line);
                }
                line.push_back(c);
                width += w;
            }
            emit(std::move(line));
            return lines;
        }

        /// <name>, or [name] where it may be left out, with an ellipsis where it repeats.
        std::string displayed(const Argument &argument) {
            std::string res = "<" + argument.displayName() + ">";
            if (argument.arity() != Argument::Single) {
                res += "...";
            }
            return argument.isRequired() ? res : "[" + res + "]";
        }

        /// The option and whatever it takes, as it would be typed.
        std::string displayed(const Option &option, bool all_spellings) {
            std::string res;
            if (all_spellings) {
                for (size_t i = 0; i < option.tokens().size(); ++i) {
                    res += (i ? ", " : "") + option.tokens()[i];
                }
            } else {
                res = option.token();
            }
            for (const auto &argument : option.arguments()) {
                res += " " + displayed(argument);
            }
            return res;
        }

        /// Puts \a names into the groups \a catalogue asks for, in the order the catalogue gives
        /// them, with whatever it does not mention left under \a fallback at the end.
        template <class T, class Name, class Line>
        std::vector<Section> grouped(const std::vector<T> &items,
                                     const std::vector<CommandCatalogue::Group> &groups,
                                     const std::string &fallback, Name name, Line line) {
            std::vector<Section> res;
            std::vector<bool> taken(items.size(), false);

            for (const auto &group : groups) {
                Section section{group.name, {}};
                for (const auto &wanted : group.members) {
                    for (size_t i = 0; i < items.size(); ++i) {
                        if (!taken[i] && name(items[i]) == wanted) {
                            section.entries.push_back(line(items[i]));
                            taken[i] = true;
                            break;
                        }
                    }
                }
                if (!section.entries.empty()) {
                    res.push_back(std::move(section));
                }
            }

            Section rest{fallback, {}};
            for (size_t i = 0; i < items.size(); ++i) {
                if (!taken[i]) {
                    rest.entries.push_back(line(items[i]));
                }
            }
            if (!rest.entries.empty()) {
                res.push_back(std::move(rest));
            }
            return res;
        }

        void writeSections(std::string &out, const std::vector<Section> &sections, size_t width,
                           int text_width) {
            // Where a description starts, and therefore where the lines under the first one are
            // indented to, so a wrapped entry stays one block instead of drifting left.
            size_t column = size_t(indent) + width + size_t(gap);
            int room = (std::max) (text_width - int(column), min_description);

            for (const auto &section : sections) {
                out += "\n" + section.title + ":\n";
                for (const auto &entry : section.entries) {
                    out += std::string(indent, ' ') + entry.left;
                    if (!entry.right.empty()) {
                        auto lines = wrapped(entry.right, room);
                        out += std::string(width - size_t(console::display_width(entry.left)) + gap,
                                           ' ') +
                               lines.front();
                        for (size_t i = 1; i < lines.size(); ++i) {
                            out += "\n" + std::string(column, ' ') + lines[i];
                        }
                    }
                    out += "\n";
                }
            }
        }

        size_t widestOf(const std::vector<Section> &sections) {
            size_t width = 0;
            for (const auto &section : sections) {
                for (const auto &entry : section.entries) {
                    // In columns rather than in bytes, or a metavar written in a script that is
                    // not ASCII pushes its own row out of line with every other.
                    width = (std::max) (width, size_t(console::display_width(entry.left)));
                }
            }
            return width;
        }

        std::string helpFor(const Command &command, const std::vector<std::string> &path,
                            const std::vector<const Option *> &inherited,
                            const std::string &prologue, const std::string &epilogue, int flags,
                            int text_width) {
            const auto &catalogue = command.catalogue();
            bool align_all = (flags & Parser::AlignAllCatalogues) != 0;

            // What an argument adds to the right hand column beyond its description. The same
            // for an argument of a command and an argument of an option, since a default value
            // is worth as much in either place.
            auto extras = [flags](const Argument &argument) {
                std::string res;
                if ((flags & Parser::ShowArgumentExpectedValues) &&
                    !argument.expectedValues().empty()) {
                    std::string words;
                    for (const auto &item : argument.expectedValues()) {
                        words += (words.empty() ? "" : ", ") + item;
                    }
                    res += " [" + words + "]";
                }
                if ((flags & Parser::ShowArgumentDefaultValue) && argument.hasDefaultValue()) {
                    res += " (default: " + argument.defaultValue() + ")";
                }
                return res;
            };
            auto argument_line = [extras](const Argument &argument) {
                return Entry{displayed(argument), argument.description() + extras(argument)};
            };
            auto option_line = [flags, extras](const Option &option) {
                std::string right = option.description();
                for (const auto &argument : option.arguments()) {
                    right += extras(argument);
                }
                if ((flags & Parser::ShowOptionIsRequired) && option.isRequired()) {
                    right += " (required)";
                }
                return Entry{displayed(option, true), right};
            };
            auto command_line = [](const Command &item) {
                return Entry{item.name(), item.description()};
            };

            auto arguments = grouped(
                command.arguments(), catalogue.argumentGroups(), "Arguments",
                [](const Argument &item) { return item.name(); }, argument_line);
            // An option with no spelling at all has nothing to print and nothing to be looked up
            // by, and token() is front() on an empty vector. A default constructed Option is one,
            // so a tree can hold one and the help text is not the place to find that out.
            std::vector<Option> named;
            for (const auto &item : command.options()) {
                if (!item.tokens().empty()) {
                    named.push_back(item);
                }
            }
            auto options = grouped(
                named, catalogue.optionGroups(), "Options",
                [](const Option &item) { return item.token(); }, option_line);
            auto commands = grouped(
                command.commands(), catalogue.commandGroups(), "Commands",
                [](const Command &item) { return item.name(); }, command_line);

            // What the commands above declared global is in scope here and is demanded here, so
            // it is listed here. Under a heading of its own, since it belongs to the program
            // rather than to this command, and since the catalogue's groups were written for
            // this command's own options and have nothing to say about these.
            std::vector<Option> globals;
            for (const auto *option : inherited) {
                if (!option->tokens().empty()) {
                    globals.push_back(*option);
                }
            }
            std::vector<Section> global_options;
            if (!globals.empty()) {
                Section section{"Global options", {}};
                for (const auto &option : globals) {
                    section.entries.push_back(option_line(option));
                }
                global_options.push_back(std::move(section));
            }

            std::string out;
            if (!prologue.empty()) {
                out += prologue + "\n";
            }
            if (!command.description().empty()) {
                out += (out.empty() ? "" : "\n") + command.description() + "\n";
            }

            // Usage, as pieces that each stay whole. An option and the value it takes are one
            // piece, since breaking between them would read as two separate things.
            std::string head;
            for (size_t i = 0; i < path.size(); ++i) {
                head += (i ? " " : "") + path[i];
            }

            // An option that has to be given is not optional information, so it is spelled out
            // where a reader looks first rather than left inside "[options]". The hint stays for
            // whatever is left, and goes away when nothing is.
            std::vector<std::string> parts;
            size_t optional_count = 0;
            for (const auto *list : {&named, &globals}) {
                for (const auto &option : *list) {
                    if (option.isRequired()) {
                        parts.push_back(displayed(option, false));
                    } else {
                        optional_count++;
                    }
                }
            }
            if (optional_count > 0) {
                parts.push_back("[options]");
            }
            for (const auto &argument : command.arguments()) {
                parts.push_back(displayed(argument));
            }
            if (!command.commands().empty()) {
                parts.push_back("[commands]");
            }

            // Wrapped like everything else, with what follows lined up under the command name
            // rather than back at the margin, so a long line still reads as one thing.
            const std::string lead = "Usage: ";
            std::string line = lead + head;
            int line_width = console::display_width(line);
            std::string usage;
            for (const auto &part : parts) {
                int part_width = console::display_width(part);
                bool at_margin = line_width == int(lead.size());
                if (!at_margin && line_width + 1 + part_width > text_width) {
                    usage += line + "\n";
                    line = std::string(lead.size(), ' ');
                    line_width = int(lead.size());
                    at_margin = true;
                }
                // At the margin the piece goes straight down under the command name. Anywhere
                // else it needs the space that separates it from what came before.
                line += at_margin ? part : " " + part;
                line_width += at_margin ? part_width : 1 + part_width;
            }
            usage += line;
            out += (out.empty() ? "" : "\n") + usage + "\n";

            if (align_all) {
                size_t width = (std::max) ({widestOf(arguments), widestOf(options),
                                            widestOf(global_options), widestOf(commands)});
                writeSections(out, arguments, width, text_width);
                writeSections(out, options, width, text_width);
                writeSections(out, global_options, width, text_width);
                writeSections(out, commands, width, text_width);
            } else {
                for (const auto *list : {&arguments, &options, &global_options, &commands}) {
                    for (const auto &section : *list) {
                        writeSections(out, {section}, widestOf({section}), text_width);
                    }
                }
            }

            if (!epilogue.empty()) {
                out += "\n" + epilogue + "\n";
            }
            return out;
        }

    }

    // ---------------------------------------------------------------------------------------
    // Parsing
    //
    // Three rules below differ from SysCmdLine, which this replaces. Each is a line SysCmdLine
    // accepts and this refuses. All three were measured against it.
    //
    // A subcommand is looked for after the options the root declared. SysCmdLine stops looking
    // at the first option, so a global option cannot be written where every program with one
    // puts it, and the tokens after it are dropped. An option belonging to the subcommand rather
    // than to the root is still unknown in front of it, which is the case that should be
    // refused.
    //
    // Positional tokens a command cannot take are an error. SysCmdLine drops them, so a mistyped
    // subcommand succeeds silently. Measured: a root declaring no arguments accepted four
    // surplus tokens and did nothing with them.
    //
    // An option that needs a value will not take a token that is a declared option of the same
    // command. Reporting it beats swallowing --force and leaving the reader to find out where it
    // went. Only a declared option counts, so a negative number or an undeclared name is still a
    // value.
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
            /// The same, for a failure that is a name spelled wrong, where the names that were
            /// declared are worth offering back.
            void failFor(Error error, std::string text, std::string token,
                         std::vector<std::string> candidates) {
                if (failed()) {
                    return;
                }
                fail(error, std::move(text));
                r->error_token = std::move(token);
                r->error_candidates = std::move(candidates);
            }

            void expandResponseFiles();
            const Command *subcommandFor(const std::string &token) const;
            void enter(const Command *next);
            void collectOptions();
            void readTokens();
            bool readOption(const std::string &token);
            bool readOneOption(OptionData *data, std::string_view inline_value);
            OptionData *lookup(std::string_view token) const;
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

        OptionData *ParserCore::lookup(std::string_view token) const {
            if (!on(Parser::IgnoreOptionCase)) {
                return r->findForWriting(token);
            }
            for (auto &item : r->options) {
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
                failFor(ParseResult::InvalidArgumentValue,
                        "\"" + token + "\" is not one of the values " + where + " accepts", token,
                        expected);
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
        bool ParserCore::readOneOption(OptionData *data, std::string_view inline_value) {
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

            data->occurrences.push_back(std::move(slots));
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
            std::vector<OptionData *> found;
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
            for (auto &item : r->options) {
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

            std::vector<std::string> declared;
            for (const auto &item : r->options) {
                for (const auto &spelling : item.option->tokens()) {
                    declared.push_back(spelling);
                }
            }
            failFor(ParseResult::UnknownOption, "unknown option \"" + token + "\"", token,
                    std::move(declared));
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
                // Nothing placed at all, on a command that has subcommands, means the token sat
                // where a subcommand goes. Saying so beats counting arguments at somebody who
                // mistyped a name.
                if (taken == 0 && !r->target->commands().empty()) {
                    std::vector<std::string> declared;
                    for (const auto &command : r->target->commands()) {
                        declared.push_back(command.name());
                    }
                    failFor(ParseResult::UnknownCommand,
                            "\"" + positional[0] + "\" is not a command of \"" +
                                r->target->name() + "\"",
                            positional[0], std::move(declared));
                    return;
                }
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
                for (auto &item : r->options) {
                    if (item.option->prior() == Option::AutoSetWhenNoSymbols) {
                        item.occurrences.emplace_back(item.option->arguments().size());
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
        int display_options = Parser::Normal;
        int text_width = 0;
    };

    Parser::Parser() : _impl(std::make_unique<Impl>()) {
    }

    Parser::Parser(Command root) : _impl(std::make_unique<Impl>()) {
        _impl->root = std::make_shared<Command>(std::move(root));
    }

    Parser::~Parser() = default;

    Parser::Parser(Parser &&other) noexcept = default;

    Parser &Parser::operator=(Parser &&other) noexcept = default;

    void Parser::setRootCommand(Command root) {
        // A new tree rather than new contents for the old one. Every ParseResult already handed
        // out shares this pointer and holds raw pointers into what it addresses, so assigning
        // through it leaves them all reading freed vectors.
        _impl->root = std::make_shared<Command>(std::move(root));
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

    void Parser::setDisplayOptions(int options) {
        _impl->display_options = options;
    }

    int Parser::displayOptions() const {
        return _impl->display_options;
    }

    void Parser::setTextWidth(int width) {
        _impl->text_width = width;
    }

    int Parser::textWidth() const {
        return _impl->text_width;
    }

    ParseResult Parser::parse(const std::vector<std::string> &args, int parseOptions) const {
        ParseResult result;
        result._impl->root = _impl->root;
        result._impl->target = _impl->root.get();
        result._impl->prologue = _impl->prologue;
        result._impl->epilogue = _impl->epilogue;
        result._impl->display_options = _impl->display_options;
        result._impl->text_width = _impl->text_width;
        ParserCore(result._impl.get(), parseOptions).run(args);
        return result;
    }

}
