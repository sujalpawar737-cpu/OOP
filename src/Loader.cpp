#include "Loader.h"

#include <fstream>

#include "Utils.h"

// ---------------- Tokenizer ----------------

Tokenizer::Tokenizer(const std::string& source) : pos_(0) {
    size_t i = 0;
    while (i < source.size()) {
        char c = source[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
        } else if (c == '(' || c == ')') {
            Token t;
            t.text = std::string(1, c);
            t.quoted = false;
            tokens_.push_back(t);
            i++;
        } else if (c == '"') {
            size_t end = source.find('"', i + 1);
            if (end == std::string::npos) {
                throw DataError("Missing closing quote in: " + source.substr(i, 40));
            }
            Token t;
            t.text = source.substr(i + 1, end - i - 1);
            t.quoted = true;
            tokens_.push_back(t);
            i = end + 1;
        } else {
            size_t start = i;
            while (i < source.size() && source[i] != ' ' && source[i] != '\t' && source[i] != '\n' &&
                   source[i] != '\r' && source[i] != '(' && source[i] != ')' && source[i] != '"') {
                i++;
            }
            Token t;
            t.text = source.substr(start, i - start);
            t.quoted = false;
            tokens_.push_back(t);
        }
    }
}

bool Tokenizer::hasMore() const {
    return pos_ < tokens_.size();
}

const Token& Tokenizer::peek() const {
    if (!hasMore()) {
        throw DataError("Unexpected end of rule or benefit text");
    }
    return tokens_[pos_];
}

Token Tokenizer::next() {
    Token t = peek();
    pos_++;
    return t;
}

bool Tokenizer::peekIs(const std::string& word) const {
    return hasMore() && !tokens_[pos_].quoted && tokens_[pos_].text == word;
}

void Tokenizer::expect(const std::string& word) {
    Token t = next();
    if (t.quoted || t.text != word) {
        throw DataError("Expected '" + word + "' but found '" + t.text + "'");
    }
}

// ---------------- RuleFactory ----------------

std::unique_ptr<Rule> RuleFactory::build(Tokenizer& tokens, const FormModel& form) {
    std::string type = tokens.next().text;

    if (type == "num") {
        const FieldDef& field = form.field(tokens.next().text);
        std::string op = tokens.next().text;
        double limit = util::toNumber(tokens.next().text, "rule limit");
        std::string label = tokens.next().text;
        if (field.type() != "number") {
            throw DataError("Number rule used on non-number field " + field.id());
        }
        return std::unique_ptr<Rule>(new NumericRule(label, &field, op, limit));
    }

    if (type == "in" || type == "notin") {
        const FieldDef& field = form.field(tokens.next().text);
        std::vector<std::string> list = util::split(tokens.next().text, ',');
        std::string label = tokens.next().text;
        if (field.type() != "select") {
            throw DataError("Set rule used on non-select field " + field.id());
        }
        std::set<std::string> values;
        for (size_t i = 0; i < list.size(); i++) {
            if (!field.hasOption(list[i])) {
                throw DataError("'" + list[i] + "' is not an option of field " + field.id());
            }
            values.insert(list[i]);
        }
        return std::unique_ptr<Rule>(new SetRule(label, &field, values, type == "notin"));
    }

    if (type == "bool") {
        const FieldDef& field = form.field(tokens.next().text);
        std::string expected = tokens.next().text;
        std::string label = tokens.next().text;
        if (field.type() != "bool") {
            throw DataError("Yes/no rule used on non yes/no field " + field.id());
        }
        return std::unique_ptr<Rule>(new BoolRule(label, &field, expected == "yes"));
    }

    if (type == "any" || type == "all") {
        std::string label = tokens.next().text;
        tokens.expect("(");
        std::vector<std::unique_ptr<Rule> > children = buildUntilClose(tokens, form);
        CompositeRule::Mode mode = (type == "all") ? CompositeRule::ALL : CompositeRule::ANY;
        return std::unique_ptr<Rule>(new CompositeRule(label, mode, std::move(children)));
    }

    throw DataError("Unknown rule type: " + type);
}

std::vector<std::unique_ptr<Rule> > RuleFactory::buildUntilClose(Tokenizer& tokens, const FormModel& form) {
    std::vector<std::unique_ptr<Rule> > rules;
    while (!tokens.peekIs(")")) {
        rules.push_back(build(tokens, form));
    }
    tokens.next();   // the ")"
    return rules;
}

// ---------------- BenefitFactory ----------------

// reads the key=value words that come before the quoted basis text
static std::map<std::string, std::string> readOptions(Tokenizer& tokens) {
    std::map<std::string, std::string> options;
    while (tokens.hasMore() && !tokens.peek().quoted && tokens.peek().text.find('=') != std::string::npos) {
        std::string word = tokens.next().text;
        size_t eq = word.find('=');
        options[word.substr(0, eq)] = word.substr(eq + 1);
    }
    return options;
}

static double numberOption(const std::map<std::string, std::string>& options,
                           const std::string& key, double fallback) {
    auto it = options.find(key);
    if (it == options.end()) {
        return fallback;
    }
    return util::toNumber(it->second, key);
}

static std::string textOption(const std::map<std::string, std::string>& options,
                              const std::string& key, const std::string& fallback) {
    auto it = options.find(key);
    return it == options.end() ? fallback : it->second;
}

static void checkNumberField(const FormModel& form, const std::string& id) {
    if (form.field(id).type() != "number") {
        throw DataError("Benefit uses a field that is not a number: " + id);
    }
}

std::unique_ptr<Benefit> BenefitFactory::build(Tokenizer& tokens, const FormModel& form) {
    std::string type = tokens.next().text;

    if (type == "fixed") {
        std::map<std::string, std::string> o = readOptions(tokens);
        std::string basis = tokens.next().text;
        double annual = numberOption(o, "annual", 0);
        int years = static_cast<int>(numberOption(o, "years", annual > 0 ? 99 : 1));
        return std::unique_ptr<Benefit>(new FixedBenefit(annual, numberOption(o, "one_time", 0), years,
                                                         numberOption(o, "cover", 0),
                                                         numberOption(o, "future_pension_monthly", 0), basis));
    }

    if (type == "service") {
        return std::unique_ptr<Benefit>(new ServiceBenefit(tokens.next().text));
    }

    if (type == "perunit" || type == "percent") {
        std::map<std::string, std::string> o = readOptions(tokens);
        std::string basis = tokens.next().text;
        std::string field = textOption(o, "field", "");
        checkNumberField(form, field);
        bool annual = textOption(o, "mode", type == "perunit" ? "annual" : "one_time") == "annual";
        int years = static_cast<int>(numberOption(o, "years", annual ? 99 : 1));
        if (type == "perunit") {
            return std::unique_ptr<Benefit>(new PerUnitBenefit(field, numberOption(o, "rate", 0),
                                                               numberOption(o, "cap_units", NO_CAP),
                                                               annual, years, basis));
        }
        return std::unique_ptr<Benefit>(new PercentBenefit(field, numberOption(o, "pct", 0),
                                                           numberOption(o, "cap", NO_CAP),
                                                           annual, years, basis));
    }

    if (type == "interest") {
        std::map<std::string, std::string> o = readOptions(tokens);
        InterestBenefit::Params p;
        p.basis = tokens.next().text;
        p.field = textOption(o, "field", "");
        if (!p.field.empty()) checkNumberField(form, p.field);
        p.yearsField = textOption(o, "years_field", "");
        if (!p.yearsField.empty()) checkNumberField(form, p.yearsField);
        p.fixedLoan = numberOption(o, "loan_fixed", 0);
        p.cap = numberOption(o, "cap", NO_CAP);
        p.percent = numberOption(o, "pct", 0);
        p.years = numberOption(o, "years", 1);
        p.extraYears = numberOption(o, "extra_years", 0);
        p.factor = numberOption(o, "factor", 1.0);
        p.recurring = numberOption(o, "recurring", 0) != 0;
        p.emiMonths = static_cast<int>(numberOption(o, "emi_months", 0));
        p.loanRate = numberOption(o, "loan_rate", 0);
        return std::unique_ptr<Benefit>(new InterestBenefit(p));
    }

    if (type == "tiered") {
        tokens.expect("(");
        std::vector<TieredBenefit::Tier> tiers;
        std::unique_ptr<Benefit> fallback;
        while (!tokens.peekIs(")")) {
            std::string word = tokens.next().text;
            if (word == "tier") {
                tokens.expect("(");
                TieredBenefit::Tier tier;
                tier.when = RuleFactory::buildUntilClose(tokens, form);
                tier.benefit = build(tokens, form);
                tiers.push_back(std::move(tier));
            } else if (word == "default") {
                fallback = build(tokens, form);
            } else {
                throw DataError("Unexpected '" + word + "' inside a tiered benefit");
            }
        }
        tokens.next();   // the ")"
        if (!fallback) {
            throw DataError("A tiered benefit needs a default");
        }
        return std::unique_ptr<Benefit>(new TieredBenefit(std::move(tiers), std::move(fallback)));
    }

    if (type == "composite") {
        std::string basis = tokens.next().text;
        tokens.expect("(");
        std::vector<std::unique_ptr<Benefit> > parts;
        while (!tokens.peekIs(")")) {
            parts.push_back(build(tokens, form));
        }
        tokens.next();   // the ")"
        return std::unique_ptr<Benefit>(new CompositeBenefit(std::move(parts), basis));
    }

    throw DataError("Unknown benefit type: " + type);
}

// ---------------- Record ----------------

bool Record::has(const std::string& key) const {
    return values.count(key) > 0;
}

std::string Record::get(const std::string& key, const std::string& fallback) const {
    auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

std::string Record::require(const std::string& key) const {
    auto it = values.find(key);
    if (it == values.end() || it->second.empty()) {
        throw DataError("line " + std::to_string(line) + ": '" + key + "' is missing");
    }
    return it->second;
}

// ---------------- DataLoader ----------------

std::vector<Record> DataLoader::readRecords(const std::string& fileName) const {
    std::string path = folder_ + "/" + fileName;
    std::ifstream in(path.c_str());
    if (!in) {
        throw DataError("Cannot open data file: " + path);
    }

    std::vector<Record> records;
    Record current;
    bool open = false;
    std::string lastKey;
    std::string line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);   // files saved on Windows
        }
        std::string text = util::trim(line);

        if (text.empty()) {
            if (open) {
                records.push_back(current);
                open = false;
            }
            continue;
        }
        if (text[0] == '#') {
            continue;
        }

        if (!open) {
            current = Record();
            current.header = util::splitWords(text);
            current.line = lineNo;
            open = true;
            lastKey = "";
            continue;
        }

        bool indented = (line[0] == ' ' || line[0] == '\t');
        if (indented) {
            if (lastKey.empty()) {
                throw DataError(path + ":" + std::to_string(lineNo) + ": unexpected indented line");
            }
            current.values[lastKey] += " " + text;
            continue;
        }

        size_t eq = text.find('=');
        if (eq == std::string::npos) {
            throw DataError(path + ":" + std::to_string(lineNo) + ": expected 'key = value'");
        }
        lastKey = util::trim(text.substr(0, eq));
        current.values[lastKey] = util::trim(text.substr(eq + 1));
    }
    if (open) {
        records.push_back(current);
    }
    return records;
}

void DataLoader::loadForm(FormModel& form) const {
    std::vector<Record> fieldRecords = readRecords("fields.txt");
    for (size_t i = 0; i < fieldRecords.size(); i++) {
        const Record& r = fieldRecords[i];
        try {
            std::unique_ptr<FieldDef> f(new FieldDef(r.header.at(1), r.require("label"), r.require("type")));
            f->setUnit(r.get("unit")).setHelp(r.get("help")).setRequired(r.get("required") == "yes");
            if (r.has("min")) f->setMin(util::toNumber(r.get("min"), "min"));
            if (r.has("max")) f->setMax(util::toNumber(r.get("max"), "max"));

            std::vector<std::string> options = util::split(r.get("options"), '|');
            for (size_t k = 0; k < options.size(); k++) {
                size_t colon = options[k].find(':');
                if (colon == std::string::npos) {
                    throw DataError("option '" + options[k] + "' needs the form value:label");
                }
                f->addOption(options[k].substr(0, colon), options[k].substr(colon + 1));
            }
            if (f->type() == "select" && options.empty()) {
                throw DataError("a select field needs options");
            }
            form.addField(std::move(f));
        } catch (const std::exception& e) {
            throw DataError("fields.txt (line " + std::to_string(r.line) + "): " + e.what());
        }
    }

    std::vector<Record> domainRecords = readRecords("domains.txt");
    for (size_t i = 0; i < domainRecords.size(); i++) {
        const Record& r = domainRecords[i];
        try {
            std::unique_ptr<DomainDef> d(new DomainDef(r.header.at(1), r.require("name"), r.get("description")));
            std::vector<std::string> ids = util::splitWords(r.require("fields"));
            for (size_t k = 0; k < ids.size(); k++) {
                d->addFieldId(ids[k]);
            }
            form.addDomain(std::move(d));
        } catch (const std::exception& e) {
            throw DataError("domains.txt (line " + std::to_string(r.line) + "): " + e.what());
        }
    }
}

void DataLoader::loadSchemes(const FormModel& form, Registry<Scheme>& schemes) const {
    std::vector<Record> records = readRecords("schemes.txt");
    for (size_t i = 0; i < records.size(); i++) {
        const Record& r = records[i];
        std::string id = r.header.size() > 1 ? r.header[1] : "?";
        try {
            std::unique_ptr<Scheme> s(new Scheme(id, r.require("name"), r.require("link")));
            s->setShortName(r.get("short", s->name()));
            s->setMinistry(r.require("ministry"));
            s->setDescription(r.require("description"));
            s->setBenefitText(r.require("benefit_text"));
            s->setNotes(r.get("notes"));
            s->setHowToApply(r.require("how"));
            s->setApplyLink(r.get("apply"));
            s->setStatusNote(r.get("status_note"));
            s->setLastChecked(r.get("checked"));
            s->setSelectionBased(r.get("selection_based") == "yes");
            s->setListBased(r.get("list_based") == "yes");

            std::vector<std::string> domains = util::splitWords(r.require("domains"));
            for (size_t k = 0; k < domains.size(); k++) {
                if (!form.hasDomain(domains[k])) {
                    throw DataError("unknown domain " + domains[k]);
                }
                s->addDomain(domains[k]);
            }

            std::vector<std::string> documents = util::split(r.get("documents"), '|');
            for (size_t k = 0; k < documents.size(); k++) {
                s->addDocument(documents[k]);
            }

            Tokenizer ruleTokens(r.get("rules"));
            while (ruleTokens.hasMore()) {
                s->addRule(RuleFactory::build(ruleTokens, form));
            }

            Tokenizer benefitTokens(r.require("benefit"));
            s->setBenefit(BenefitFactory::build(benefitTokens, form));
            if (benefitTokens.hasMore()) {
                throw DataError("extra text after the benefit");
            }
            schemes.add(std::move(s));
        } catch (const std::exception& e) {
            throw DataError("schemes.txt, scheme " + id + ": " + e.what());
        }
    }
}

void DataLoader::loadConflicts(ConflictRegistry& conflicts) const {
    std::vector<Record> records = readRecords("conflicts.txt");
    for (size_t i = 0; i < records.size(); i++) {
        const Record& r = records[i];
        try {
            if (r.header.size() < 3) {
                throw DataError("a conflict header needs: conflict <exclusive|note> <official|conservative>");
            }
            ConflictType type = (r.header[1] == "note") ? ConflictType::Note : ConflictType::Exclusive;
            std::string basis = r.header[2];
            std::string reason = r.require("reason");
            std::string source = r.get("source");

            // pairs = a b | c d    (each pair on its own)
            std::vector<std::string> pairs = util::split(r.get("pairs"), '|');
            for (size_t k = 0; k < pairs.size(); k++) {
                std::vector<std::string> two = util::splitWords(pairs[k]);
                if (two.size() != 2) throw DataError("a pair needs exactly two scheme ids");
                conflicts.add(Conflict(two[0], two[1], type, basis, reason, source));
            }

            // group = a b c       (every scheme conflicts with every other one in the group)
            std::vector<std::string> group = util::splitWords(r.get("group"));
            for (size_t x = 0; x < group.size(); x++) {
                for (size_t y = x + 1; y < group.size(); y++) {
                    conflicts.add(Conflict(group[x], group[y], type, basis, reason, source));
                }
            }

            // cross = a b | c d   (every one on the left conflicts with every one on the right)
            std::vector<std::string> sides = util::split(r.get("cross"), '|');
            if (!sides.empty()) {
                if (sides.size() != 2) throw DataError("cross needs two lists separated by |");
                std::vector<std::string> left = util::splitWords(sides[0]);
                std::vector<std::string> right = util::splitWords(sides[1]);
                for (size_t x = 0; x < left.size(); x++) {
                    for (size_t y = 0; y < right.size(); y++) {
                        conflicts.add(Conflict(left[x], right[y], type, basis, reason, source));
                    }
                }
            }
        } catch (const std::exception& e) {
            throw DataError("conflicts.txt (line " + std::to_string(r.line) + "): " + e.what());
        }
    }
}

std::map<std::string, std::string> DataLoader::loadMeta() const {
    std::vector<Record> records = readRecords("meta.txt");
    if (records.empty()) {
        throw DataError("meta.txt is empty");
    }
    return records[0].values;
}
