#include "Combination.h"

#include <algorithm>
#include <functional>

#include "Utils.h"

// ---------------- Conflict ----------------

Conflict::Conflict() : type(ConflictType::Exclusive) {}

Conflict::Conflict(const std::string& a_, const std::string& b_, ConflictType type_,
                   const std::string& basis_, const std::string& reason_, const std::string& source_)
    : a(a_), b(b_), type(type_), basis(basis_), reason(reason_), source(source_) {}

std::string Conflict::toJson() const {
    JsonObject json;
    json.add("a", a).add("b", b);
    json.add("type", type == ConflictType::Exclusive ? "exclusive" : "note");
    json.add("basis", basis).add("reason", reason).add("source", source);
    return json.str();
}

// ---------------- ConflictRegistry ----------------

std::string ConflictRegistry::makeKey(const std::string& a, const std::string& b) {
    return a < b ? a + "|" + b : b + "|" + a;   // same key whichever order the two ids come in
}

void ConflictRegistry::add(const Conflict& conflict) {
    if (conflict.a == conflict.b) {
        return;
    }
    conflicts_[makeKey(conflict.a, conflict.b)] = conflict;
}

const Conflict* ConflictRegistry::find(const std::string& a, const std::string& b) const {
    auto it = conflicts_.find(makeKey(a, b));
    if (it == conflicts_.end()) {
        return nullptr;
    }
    return &it->second;
}

void ConflictRegistry::checkIds(const std::set<std::string>& knownIds) const {
    for (auto it = conflicts_.begin(); it != conflicts_.end(); ++it) {
        const Conflict& c = it->second;
        if (knownIds.count(c.a) == 0 || knownIds.count(c.b) == 0) {
            throw DataError("conflicts.txt mentions an unknown scheme: " + c.a + " / " + c.b);
        }
    }
}

// ---------------- Combination ----------------

bool Combination::operator>(const Combination& other) const {
    if (score != other.score) {
        return score > other.score;
    }
    if (members.size() != other.members.size()) {
        return members.size() > other.members.size();   // tie: prefer more schemes
    }
    return mask < other.mask;
}

// ---------------- CombinationEngine ----------------

double CombinationEngine::score(const BenefitValue& value, const Preferences& pref) const {
    double money = value.moneyOver(pref.horizon);
    if (pref.objective == "cover") {
        return value.cover + money * 1e-6;
    }
    if (pref.objective == "balanced") {
        return money + 0.1 * value.cover;
    }
    return money + value.cover * 1e-6;   // the tiny cover term only breaks ties
}

void CombinationEngine::remember(SearchState& s, unsigned mask, double score, int count) const {
    s.top.push_back(std::make_pair(score + count * 1e-9, mask));
    std::sort(s.top.begin(), s.top.end(),
              [](const std::pair<double, unsigned>& x, const std::pair<double, unsigned>& y) {
                  if (x.first != y.first) return x.first > y.first;
                  return x.second < y.second;
              });
    if (s.top.size() > s.keep) {
        s.top.pop_back();
    }
}

// recursion: for every scheme either leave it out, or take it if it does not clash
// with something already taken
void CombinationEngine::search(SearchState& s, int index, unsigned mask, double score, int count) const {
    if (index == s.n) {
        if (mask != 0) {
            remember(s, mask, score, count);
        }
        return;
    }
    search(s, index + 1, mask, score, count);
    if ((s.blocked[index] & mask) == 0) {
        search(s, index + 1, mask | (1u << index), score + s.scores[index], count + 1);
    }
}

std::vector<Combination> CombinationEngine::best(const std::vector<const Assessment*>& candidates,
                                                 const Preferences& pref, size_t howMany) const {
    SearchState s;
    s.n = static_cast<int>(candidates.size());
    s.keep = howMany;
    if (s.n > 22) {
        throw DataError("Too many schemes to try every combination.");
    }

    // blocked[i] has a bit set for every scheme that cannot go together with scheme i
    s.blocked.assign(s.n, 0);
    s.scores.assign(s.n, 0);
    for (int i = 0; i < s.n; i++) {
        s.scores[i] = score(candidates[i]->value, pref);
        for (int k = 0; k < s.n; k++) {
            if (i == k) continue;
            const Conflict* c = registry_.find(candidates[i]->scheme->id(), candidates[k]->scheme->id());
            if (c != nullptr && c->type == ConflictType::Exclusive) {
                s.blocked[i] |= (1u << k);
            }
        }
    }

    search(s, 0, 0, 0.0, 0);

    std::vector<Combination> result;
    for (size_t t = 0; t < s.top.size(); t++) {
        Combination c;
        c.mask = s.top[t].second;
        for (int i = 0; i < s.n; i++) {
            if (c.mask & (1u << i)) {
                c.members.push_back(i);
                c.money += candidates[i]->value.moneyOver(pref.horizon);
                c.cover += candidates[i]->value.cover;
                c.futurePension += candidates[i]->value.futurePension;
                c.score += s.scores[i];
            }
        }
        result.push_back(c);
    }
    std::sort(result.begin(), result.end(), std::greater<Combination>());
    return result;
}

std::string CombinationEngine::pairReport(const std::vector<const Scheme*>& chosen) const {
    std::string list = "[";
    bool first = true;
    for (size_t i = 0; i < chosen.size(); i++) {
        for (size_t k = i + 1; k < chosen.size(); k++) {
            JsonObject pair;
            pair.add("a", chosen[i]->id()).add("b", chosen[k]->id());
            pair.add("a_name", chosen[i]->shortName()).add("b_name", chosen[k]->shortName());

            const Conflict* c = registry_.find(chosen[i]->id(), chosen[k]->id());
            if (c == nullptr) {
                pair.add("status", "no_record");
                pair.add("reason", "No restriction is recorded in this prototype's dataset. "
                                   "This is not a guarantee - confirm with the scheme authorities.");
            } else {
                pair.add("status", c->type == ConflictType::Exclusive ? "conflict" : "note");
                pair.add("basis", c->basis).add("reason", c->reason).add("source", c->source);
            }
            if (!first) list += ",";
            list += pair.str();
            first = false;
        }
    }
    return list + "]";
}
