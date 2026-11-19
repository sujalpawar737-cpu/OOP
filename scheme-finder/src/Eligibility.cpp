#include "Eligibility.h"

#include "Utils.h"

std::string statusCode(Status status) {
    if (status == Status::Eligible) return "eligible";
    if (status == Status::Potential) return "potential";
    return "not_eligible";
}

std::string statusHeadline(Status status) {
    if (status == Status::Eligible) return "Eligible based on the information provided";
    if (status == Status::Potential) return "Potentially eligible; verify with the official authority";
    return "Not eligible based on the information provided";
}

Assessment::Assessment() : scheme(nullptr), status(Status::NotEligible), horizon(3) {}

bool Assessment::usable(bool includePotential) const {
    return status == Status::Eligible || (includePotential && status == Status::Potential);
}

std::string Assessment::toJson() const {
    // start from the scheme details, then add the result for this citizen
    std::string base = scheme->toJson();
    base = base.substr(0, base.size() - 1);   // remove the closing }

    JsonObject extra;
    extra.add("status", statusCode(status)).add("headline", statusHeadline(status));
    extra.addRaw("criteria_results", jsonArray(criteria));
    extra.addRaw("missing", jsonStrings(missing));
    extra.addRaw("failed", jsonStrings(failed));
    extra.addRaw("caveats", jsonStrings(caveats));
    if (status != Status::NotEligible) {
        extra.addRaw("value", value.toJson(horizon));
    }
    std::string more = extra.str();
    return base + "," + more.substr(1);       // join the two objects
}

Assessment EligibilityEngine::assess(const Scheme& scheme, const UserProfile& profile, int horizon) const {
    Assessment a;
    a.scheme = &scheme;
    a.horizon = horizon;

    bool anyFail = false;
    bool anyUnknown = false;
    for (size_t i = 0; i < scheme.rules().size(); i++) {
        RuleResult r = scheme.rules()[i]->evaluate(profile);
        if (r.outcome == Outcome::Fail) {
            anyFail = true;
            a.failed.push_back(r.label);
        } else if (r.outcome == Outcome::Unknown) {
            anyUnknown = true;
            a.missing.push_back(r.label);
        }
        a.criteria.push_back(r);
    }

    if (anyFail) {
        a.status = Status::NotEligible;
    } else if (anyUnknown) {
        a.status = Status::Potential;
    } else {
        a.status = Status::Eligible;
        if (scheme.selectionBased()) {
            a.status = Status::Potential;
            a.caveats.push_back("Selection is competitive or quota-based, so meeting the criteria does not guarantee an award.");
        }
        if (scheme.listBased()) {
            a.status = Status::Potential;
            a.caveats.push_back("Inclusion depends on an official beneficiary list / survey / local verification that this system cannot see.");
        }
    }

    // only estimate the benefit for schemes the person could actually receive
    if (a.status != Status::NotEligible) {
        a.value = scheme.benefit().evaluate(profile);
    }
    return a;
}

std::vector<Assessment> EligibilityEngine::assessAll(const std::vector<const Scheme*>& schemes,
                                                     const UserProfile& profile, int horizon) const {
    std::vector<Assessment> all;
    for (size_t i = 0; i < schemes.size(); i++) {
        all.push_back(assess(*schemes[i], profile, horizon));
    }
    return all;
}
