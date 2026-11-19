// Combination.h - which schemes can be used together, what the total benefit is,
// and which valid combination is best for this person.
// Every restriction is stored as a Conflict that records the reason, a source link and
// how sure we are: "official" (found in scheme guidelines) or "conservative" (cautious reading).
#ifndef COMBINATION_H
#define COMBINATION_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "Base.h"
#include "Eligibility.h"

enum class ConflictType { Exclusive, Note };   // Exclusive = cannot combine, Note = allowed with an adjustment

class Conflict : public JsonSerializable {
public:
    Conflict();
    Conflict(const std::string& a, const std::string& b, ConflictType type, const std::string& basis,
             const std::string& reason, const std::string& source);

    std::string a;
    std::string b;
    ConflictType type;
    std::string basis;
    std::string reason;
    std::string source;

    std::string toJson() const override;
};

class ConflictRegistry {
public:
    void add(const Conflict& conflict);
    const Conflict* find(const std::string& a, const std::string& b) const;   // nullptr if none recorded
    void checkIds(const std::set<std::string>& knownIds) const;               // throws DataError
    size_t size() const { return conflicts_.size(); }

private:
    static std::string makeKey(const std::string& a, const std::string& b);
    std::map<std::string, Conflict> conflicts_;
};

// what the person wants from the recommendation
struct Preferences {
    std::string objective;      // money | cover | balanced
    int horizon;                // planning period in years: 1, 3 or 5
    bool includePotential;      // also use "potentially eligible" schemes?
    Preferences() : objective("money"), horizon(3), includePotential(true) {}
};

// one conflict-free set of schemes
struct Combination {
    std::vector<int> members;   // positions in the candidate list
    double money;
    double cover;
    double futurePension;
    double score;
    unsigned mask;              // the same members as bits
    Combination() : money(0), cover(0), futurePension(0), score(0), mask(0) {}

    // lets us sort combinations from best to worst with std::greater
    bool operator>(const Combination& other) const;
};

class CombinationEngine {
public:
    explicit CombinationEngine(const ConflictRegistry& registry) : registry_(registry) {}

    double score(const BenefitValue& value, const Preferences& pref) const;

    // the best valid combinations (try every subset, skip the ones that contain a conflict)
    std::vector<Combination> best(const std::vector<const Assessment*>& candidates,
                                  const Preferences& pref, size_t howMany) const;

    // checks every pair in a chosen set of schemes
    std::string pairReport(const std::vector<const Scheme*>& chosen) const;

private:
    // working data for one call of best(); kept local so two requests never share it
    struct SearchState {
        int n;
        std::vector<unsigned> blocked;
        std::vector<double> scores;
        std::vector<std::pair<double, unsigned> > top;
        size_t keep;
    };

    void search(SearchState& s, int index, unsigned mask, double score, int count) const;
    void remember(SearchState& s, unsigned mask, double score, int count) const;

    const ConflictRegistry& registry_;
};

#endif
