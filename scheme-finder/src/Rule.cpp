#include "Rule.h"

#include "Utils.h"

std::string outcomeName(Outcome outcome) {
    if (outcome == Outcome::Pass) return "pass";
    if (outcome == Outcome::Fail) return "fail";
    return "unknown";
}

std::string RuleResult::toJson() const {
    JsonObject json;
    json.add("label", label).add("result", outcomeName(outcome)).add("detail", detail);
    if (!children.empty()) {
        json.addRaw("children", jsonArray(children));
    }
    return json.str();
}

RuleResult Rule::makeResult(Outcome outcome, const std::string& detail) const {
    RuleResult r;
    r.outcome = outcome;
    r.label = label_;
    r.detail = detail;
    return r;
}

// ---------------- NumericRule ----------------

NumericRule::NumericRule(const std::string& label, const FieldDef* field,
                         const std::string& op, double limit)
    : Rule(label), field_(field), op_(op), limit_(limit) {
    if (op != ">=" && op != "<=" && op != ">" && op != "<" && op != "==") {
        throw DataError("Unknown comparison '" + op + "' in rule: " + label);
    }
}

std::string NumericRule::opText() const {
    if (op_ == ">=") return "at least";
    if (op_ == "<=") return "at most";
    if (op_ == ">") return "more than";
    if (op_ == "<") return "less than";
    return "exactly";
}

RuleResult NumericRule::evaluate(const UserProfile& profile) const {
    double value = 0;
    if (!profile.getNumber(field_->id(), value)) {
        return makeResult(Outcome::Unknown, "Not provided - needed to confirm this requirement.");
    }

    bool ok = false;
    if (op_ == ">=") ok = value >= limit_;
    else if (op_ == "<=") ok = value <= limit_;
    else if (op_ == ">") ok = value > limit_;
    else if (op_ == "<") ok = value < limit_;
    else ok = value == limit_;

    std::string entered = "You entered " + util::formatValue(value, field_->unit());
    if (ok) {
        return makeResult(Outcome::Pass, entered + ".");
    }
    return makeResult(Outcome::Fail, entered + "; requirement is " + opText() + " " +
                                         util::formatValue(limit_, field_->unit()) + ".");
}

// ---------------- SetRule ----------------

SetRule::SetRule(const std::string& label, const FieldDef* field,
                 const std::set<std::string>& values, bool negate)
    : Rule(label), field_(field), values_(values), negate_(negate) {}

RuleResult SetRule::evaluate(const UserProfile& profile) const {
    std::string chosen;
    if (!profile.getText(field_->id(), chosen)) {
        return makeResult(Outcome::Unknown, "Not provided - needed to confirm this requirement.");
    }
    bool inside = values_.count(chosen) > 0;
    bool ok = negate_ ? !inside : inside;
    return makeResult(ok ? Outcome::Pass : Outcome::Fail,
                      "You selected: " + field_->optionLabel(chosen) + ".");
}

// ---------------- BoolRule ----------------

BoolRule::BoolRule(const std::string& label, const FieldDef* field, bool expected)
    : Rule(label), field_(field), expected_(expected) {}

RuleResult BoolRule::evaluate(const UserProfile& profile) const {
    bool answer = false;
    if (!profile.getFlag(field_->id(), answer)) {
        return makeResult(Outcome::Unknown, "Answered 'Not sure' or left blank - needed to confirm.");
    }
    return makeResult(answer == expected_ ? Outcome::Pass : Outcome::Fail,
                      std::string("You answered: ") + (answer ? "Yes." : "No."));
}

// ---------------- CompositeRule ----------------

CompositeRule::CompositeRule(const std::string& label, Mode mode,
                             std::vector<std::unique_ptr<Rule> > children)
    : Rule(label), mode_(mode), children_(std::move(children)) {}

RuleResult CompositeRule::evaluate(const UserProfile& profile) const {
    RuleResult result;
    result.label = label_;

    bool anyPass = false, anyFail = false, anyUnknown = false;
    std::string firstPass, firstFail;

    for (size_t i = 0; i < children_.size(); i++) {
        RuleResult child = children_[i]->evaluate(profile);
        if (child.outcome == Outcome::Pass) {
            anyPass = true;
            if (firstPass.empty()) firstPass = child.label;
        } else if (child.outcome == Outcome::Fail) {
            anyFail = true;
            if (firstFail.empty()) firstFail = child.label;
        } else {
            anyUnknown = true;
        }
        result.children.push_back(child);
    }

    if (mode_ == ALL) {
        if (anyFail) {
            result.outcome = Outcome::Fail;
            result.detail = "Not met: " + firstFail;
        } else if (anyUnknown) {
            result.outcome = Outcome::Unknown;
            result.detail = "Some conditions need more information.";
        } else {
            result.outcome = Outcome::Pass;
            result.detail = "All conditions are met.";
        }
    } else {
        if (anyPass) {
            result.outcome = Outcome::Pass;
            result.detail = "Met through: " + firstPass;
        } else if (anyUnknown) {
            result.outcome = Outcome::Unknown;
            result.detail = "Cannot tell yet - some alternatives need more information.";
        } else {
            result.outcome = Outcome::Fail;
            result.detail = "None of the accepted alternatives is met.";
        }
    }
    return result;
}
