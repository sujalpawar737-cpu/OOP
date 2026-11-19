#include "Api.h"

#include <algorithm>
#include <set>

#include "Utils.h"

ApiController::ApiController(const SchemeRepository& repo, UsageLog& log)
    : repo_(repo), log_(log), combos_(repo.conflicts()) {}

std::string ApiController::meta() const {
    JsonObject json;
    json.add("title", repo_.meta("title")).add("version", repo_.meta("version"));
    json.add("dataset_reviewed", repo_.meta("dataset_reviewed"));
    json.add("disclaimer", repo_.meta("disclaimer"));
    return json.str();
}

// the list of domains with the questions that belong to each
std::string ApiController::domains() const {
    std::string list = "[";
    std::vector<std::string> ids = repo_.form().domainIds();
    for (size_t i = 0; i < ids.size(); i++) {
        const DomainDef& d = repo_.form().domain(ids[i]);

        std::string fields = "[";
        for (size_t k = 0; k < d.fieldIds().size(); k++) {
            if (k > 0) fields += ",";
            fields += repo_.form().field(d.fieldIds()[k]).toJson();
        }
        fields += "]";

        std::vector<std::string> names;
        std::vector<const Scheme*> schemes = repo_.inDomain(d.id());
        for (size_t k = 0; k < schemes.size(); k++) {
            names.push_back(schemes[k]->shortName());
        }

        JsonObject json;
        json.add("id", d.id()).add("name", d.name()).add("description", d.description());
        json.add("scheme_count", static_cast<int>(names.size()));
        json.addRaw("scheme_names", jsonStrings(names)).addRaw("fields", fields);
        if (i > 0) list += ",";
        list += json.str();
    }
    return list + "]";
}

// all schemes (or one domain) with an optional search word
std::string ApiController::directory(const std::string& domain, const std::string& query) const {
    std::vector<const Scheme*> schemes;
    if (domain.empty() || domain == "all") {
        schemes = repo_.allSchemes();
    } else {
        schemes = repo_.inDomain(domain);
    }

    std::string word = util::toLower(query);
    std::string list = "[";
    bool first = true;
    for (size_t i = 0; i < schemes.size(); i++) {
        const Scheme& s = *schemes[i];
        std::string text = util::toLower(s.name() + " " + s.shortName() + " " + s.description() + " " + s.ministry());
        if (!word.empty() && text.find(word) == std::string::npos) {
            continue;
        }
        if (!first) list += ",";
        list += s.toJson();
        first = false;
    }
    return list + "]";
}

std::string ApiController::stats() {
    return log_.statsJson();
}

Preferences ApiController::readPreferences(const Params& params) const {
    Preferences pref;
    auto objective = params.find("objective");
    if (objective != params.end() &&
        (objective->second == "money" || objective->second == "cover" || objective->second == "balanced")) {
        pref.objective = objective->second;
    }
    auto horizon = params.find("horizon");
    if (horizon != params.end() && (horizon->second == "1" || horizon->second == "3" || horizon->second == "5")) {
        pref.horizon = std::stoi(horizon->second);
    }
    auto include = params.find("include_potential");
    if (include != params.end() && include->second == "false") {
        pref.includePotential = false;
    }
    return pref;
}

std::string ApiController::errorJson(const InputError& e) const {
    JsonObject json;
    json.add("error", e.what()).addRaw("details", jsonStrings(e.problems()));
    return json.str();
}

// POST /api/evaluate
std::string ApiController::evaluate(const Params& params, int& status) {
    status = 200;
    try {
        std::string domain;
        auto d = params.find("domain");
        if (d != params.end()) {
            domain = d->second;
        }
        UserProfile profile = repo_.form().buildProfile(domain, params);   // throws InputError
        Preferences pref = readPreferences(params);

        std::vector<Assessment> all = engine_.assessAll(repo_.inDomain(domain), profile, pref.horizon);

        // eligible first, then potential, then not eligible; the richer scheme first inside a group
        std::stable_sort(all.begin(), all.end(), [&](const Assessment& a, const Assessment& b) {
            if (a.status != b.status) {
                return static_cast<int>(a.status) < static_cast<int>(b.status);
            }
            return a.value.moneyOver(pref.horizon) > b.value.moneyOver(pref.horizon);
        });

        int eligible = 0, potential = 0, notEligible = 0;
        std::vector<const Assessment*> candidates;
        for (size_t i = 0; i < all.size(); i++) {
            if (all[i].status == Status::Eligible) eligible++;
            else if (all[i].status == Status::Potential) potential++;
            else notEligible++;
            if (all[i].usable(pref.includePotential)) {
                candidates.push_back(&all[i]);
            }
        }
        log_.add(domain, eligible, potential, notEligible);

        JsonObject summary;
        summary.add("eligible", eligible).add("potential", potential);
        summary.add("not_eligible", notEligible).add("total", static_cast<int>(all.size()));

        JsonObject prefJson;
        prefJson.add("objective", pref.objective).add("horizon", pref.horizon);
        prefJson.add("include_potential", pref.includePotential);

        JsonObject json;
        json.add("domain", domain).add("domain_name", repo_.form().domain(domain).name());
        json.addRaw("summary", summary.str()).addRaw("preferences", prefJson.str());
        json.addRaw("schemes", jsonArray(all));
        json.addRaw("restrictions", restrictions(candidates));
        json.addRaw("recommendation", recommendation(candidates, pref));
        json.add("disclaimer", repo_.meta("disclaimer"));
        return json.str();
    } catch (const InputError& e) {
        status = e.problems().empty() ? 400 : 422;
        return errorJson(e);
    }
}

// restrictions that apply between the schemes open to this person
std::string ApiController::restrictions(const std::vector<const Assessment*>& candidates) const {
    std::string list = "[";
    bool first = true;
    for (size_t i = 0; i < candidates.size(); i++) {
        for (size_t k = i + 1; k < candidates.size(); k++) {
            const Scheme* a = candidates[i]->scheme;
            const Scheme* b = candidates[k]->scheme;
            const Conflict* c = repo_.conflicts().find(a->id(), b->id());
            if (c == nullptr) {
                continue;
            }
            std::string base = c->toJson();
            base = base.substr(0, base.size() - 1);
            JsonObject names;
            names.add("a_name", a->shortName()).add("b_name", b->shortName());
            if (!first) list += ",";
            list += base + "," + names.str().substr(1);
            first = false;
        }
    }
    return list + "]";
}

std::string ApiController::recommendation(const std::vector<const Assessment*>& candidates,
                                          const Preferences& pref) const {
    JsonObject json;
    json.add("objective", pref.objective).add("horizon", pref.horizon);
    json.add("include_potential", pref.includePotential);
    json.add("candidate_count", static_cast<int>(candidates.size()));

    if (candidates.empty()) {
        json.addRaw("best", "null");
        return json.str();
    }

    // what the total would look like if the restrictions were ignored
    double naiveMoney = 0, naiveCover = 0;
    for (size_t i = 0; i < candidates.size(); i++) {
        naiveMoney += candidates[i]->value.moneyOver(pref.horizon);
        naiveCover += candidates[i]->value.cover;
    }
    JsonObject naive;
    naive.add("money", naiveMoney).add("cover", naiveCover);
    json.addRaw("naive", naive.str());

    std::vector<Combination> top = combos_.best(candidates, pref, 3);

    // turns one Combination into JSON
    auto describe = [&](const Combination& c) {
        std::vector<std::string> ids, names;
        for (size_t i = 0; i < c.members.size(); i++) {
            ids.push_back(candidates[c.members[i]]->scheme->id());
            names.push_back(candidates[c.members[i]]->scheme->shortName());
        }
        JsonObject o;
        o.addRaw("scheme_ids", jsonStrings(ids)).addRaw("names", jsonStrings(names));
        o.add("money", c.money).add("cover", c.cover).add("future_pension_monthly", c.futurePension);
        o.add("count", static_cast<int>(c.members.size()));
        return o.str();
    };

    json.addRaw("best", describe(top[0]));
    std::string alternatives = "[";
    for (size_t i = 1; i < top.size(); i++) {
        if (i > 1) alternatives += ",";
        alternatives += describe(top[i]);
    }
    json.addRaw("alternatives", alternatives + "]");

    // schemes that did not make it into the best combination, and which restriction kept them out
    std::set<int> inBest(top[0].members.begin(), top[0].members.end());
    std::string excluded = "[";
    bool firstExcluded = true;
    for (int i = 0; i < static_cast<int>(candidates.size()); i++) {
        if (inBest.count(i) > 0) {
            continue;
        }
        std::string because = "[";
        bool firstReason = true;
        for (size_t m = 0; m < top[0].members.size(); m++) {
            const Assessment* other = candidates[top[0].members[m]];
            const Conflict* c = repo_.conflicts().find(candidates[i]->scheme->id(), other->scheme->id());
            if (c != nullptr && c->type == ConflictType::Exclusive) {
                JsonObject r;
                r.add("with_id", other->scheme->id()).add("with_name", other->scheme->shortName());
                r.add("reason", c->reason).add("basis", c->basis);
                if (!firstReason) because += ",";
                because += r.str();
                firstReason = false;
            }
        }
        JsonObject e;
        e.add("id", candidates[i]->scheme->id()).add("name", candidates[i]->scheme->shortName());
        e.addRaw("because", because + "]");
        if (!firstExcluded) excluded += ",";
        excluded += e.str();
        firstExcluded = false;
    }
    json.addRaw("excluded", excluded + "]");
    return json.str();
}

// POST /api/combine - can the schemes the person ticked be used together?
std::string ApiController::combine(const Params& params, int& status) const {
    status = 200;
    try {
        std::string domain;
        auto d = params.find("domain");
        if (d != params.end()) {
            domain = d->second;
        }
        UserProfile profile = repo_.form().buildProfile(domain, params);
        Preferences pref = readPreferences(params);

        std::vector<std::string> ids;
        auto idsParam = params.find("scheme_ids");
        if (idsParam != params.end()) {
            ids = util::split(idsParam->second, ',');
        }

        std::vector<const Scheme*> chosen;
        std::vector<Assessment> assessed;
        std::set<std::string> seen;
        for (size_t i = 0; i < ids.size(); i++) {
            const Scheme* s = repo_.find(ids[i]);
            if (s == nullptr || !s->inDomain(domain)) {
                throw InputError("Unknown scheme in selection.");
            }
            if (seen.insert(s->id()).second) {
                chosen.push_back(s);
                assessed.push_back(engine_.assess(*s, profile, pref.horizon));
            }
        }
        if (chosen.size() < 2) {
            throw InputError("Select at least two schemes to check a combination.");
        }

        bool anyConflict = false, anyNotEligible = false;
        std::vector<std::string> notEligible;
        double money = 0, cover = 0, pension = 0;
        for (size_t i = 0; i < assessed.size(); i++) {
            if (assessed[i].status == Status::NotEligible) {
                anyNotEligible = true;
                notEligible.push_back(assessed[i].scheme->shortName());
                continue;
            }
            money += assessed[i].value.moneyOver(pref.horizon);
            cover += assessed[i].value.cover;
            pension += assessed[i].value.futurePension;
        }

        for (size_t i = 0; i < chosen.size(); i++) {
            for (size_t k = i + 1; k < chosen.size(); k++) {
                const Conflict* c = repo_.conflicts().find(chosen[i]->id(), chosen[k]->id());
                if (c != nullptr && c->type == ConflictType::Exclusive) {
                    anyConflict = true;
                }
            }
        }

        std::string verdict;
        if (anyConflict) {
            verdict = "Cannot be combined - at least one pair is restricted.";
        } else if (anyNotEligible) {
            verdict = "Not all selected schemes are open to you based on the information provided.";
        } else {
            verdict = "No restriction is recorded between these schemes in this dataset; verify with the authorities before applying.";
        }

        JsonObject totals;
        totals.add("money", money).add("cover", cover).add("future_pension_monthly", pension);
        totals.add("horizon", pref.horizon);

        JsonObject json;
        json.add("verdict", verdict).add("has_conflict", anyConflict).add("has_ineligible", anyNotEligible);
        json.addRaw("not_eligible", jsonStrings(notEligible));
        json.addRaw("pairs", combos_.pairReport(chosen));
        json.addRaw("totals", totals.str());
        json.add("note", "Totals count only the selected schemes that are eligible or potentially eligible.");
        return json.str();
    } catch (const InputError& e) {
        status = e.problems().empty() ? 400 : 422;
        return errorJson(e);
    }
}
