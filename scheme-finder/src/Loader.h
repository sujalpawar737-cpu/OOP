// Loader.h - reads the text files in data/ and builds the objects.
//
// File layout: a record starts with a header line (e.g. "scheme pm_kisan"), followed by
// "key = value" lines. A line that starts with spaces continues the previous value.
// Records are separated by a blank line. Lines starting with # are ignored.
//
// Rules and benefits are written as a small expression, for example
//     any "label" ( num age >= 18 "label"   in category sc,st "label" )
// The Tokenizer cuts it into pieces and the factories turn the pieces into objects.
#ifndef LOADER_H
#define LOADER_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Benefit.h"
#include "Combination.h"
#include "Form.h"
#include "Rule.h"
#include "Scheme.h"

struct Token {
    std::string text;
    bool quoted;
};

class Tokenizer {
public:
    explicit Tokenizer(const std::string& source);

    bool hasMore() const;
    const Token& peek() const;
    Token next();
    void expect(const std::string& word);   // the next token must be this word
    bool peekIs(const std::string& word) const;

private:
    std::vector<Token> tokens_;
    size_t pos_;
};

class RuleFactory {
public:
    static std::unique_ptr<Rule> build(Tokenizer& tokens, const FormModel& form);
    // reads rules until the closing ")"
    static std::vector<std::unique_ptr<Rule> > buildUntilClose(Tokenizer& tokens, const FormModel& form);
};

class BenefitFactory {
public:
    static std::unique_ptr<Benefit> build(Tokenizer& tokens, const FormModel& form);
};

// one block of a data file
struct Record {
    std::vector<std::string> header;
    std::map<std::string, std::string> values;
    int line;

    Record() : line(0) {}
    bool has(const std::string& key) const;
    std::string get(const std::string& key, const std::string& fallback = "") const;
    std::string require(const std::string& key) const;   // throws DataError if missing
};

class DataLoader {
public:
    explicit DataLoader(const std::string& folder) : folder_(folder) {}

    void loadForm(FormModel& form) const;
    void loadSchemes(const FormModel& form, Registry<Scheme>& schemes) const;
    void loadConflicts(ConflictRegistry& conflicts) const;
    std::map<std::string, std::string> loadMeta() const;

private:
    std::vector<Record> readRecords(const std::string& fileName) const;
    std::string folder_;
};

#endif
