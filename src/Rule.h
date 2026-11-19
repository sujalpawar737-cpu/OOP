// Rule.h - eligibility criteria as objects.
// Every rule answers one question: for this citizen is the requirement
// Pass, Fail or Unknown (answer missing)? Unknown never counts as eligible.
#ifndef RULE_H
#define RULE_H

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Base.h"
#include "Form.h"
#include "UserProfile.h"

enum class Outcome { Pass, Fail, Unknown };

std::string outcomeName(Outcome outcome);

// what one rule decided, with a sentence explaining why
struct RuleResult : public JsonSerializable {
    Outcome outcome;
    std::string label;
    std::string detail;
    std::vector<RuleResult> children;   // filled only by composite rules

    RuleResult() : outcome(Outcome::Unknown) {}
    std::string toJson() const override;
};

// abstract base class
class Rule {
public:
    explicit Rule(const std::string& label) : label_(label) {}
    virtual ~Rule() {}

    virtual RuleResult evaluate(const UserProfile& profile) const = 0;
    const std::string& label() const { return label_; }

protected:
    RuleResult makeResult(Outcome outcome, const std::string& detail) const;
    std::string label_;
};

// age >= 18, income <= 250000 ...
class NumericRule : public Rule {
public:
    NumericRule(const std::string& label, const FieldDef* field, const std::string& op, double limit);
    RuleResult evaluate(const UserProfile& profile) const override;

private:
    std::string opText() const;
    const FieldDef* field_;
    std::string op_;
    double limit_;
};

// category in {sc, st}, or "not in" when negate is true
class SetRule : public Rule {
public:
    SetRule(const std::string& label, const FieldDef* field, const std::set<std::string>& values, bool negate);
    RuleResult evaluate(const UserProfile& profile) const override;

private:
    const FieldDef* field_;
    std::set<std::string> values_;
    bool negate_;
};

// yes / no question
class BoolRule : public Rule {
public:
    BoolRule(const std::string& label, const FieldDef* field, bool expected);
    RuleResult evaluate(const UserProfile& profile) const override;

private:
    const FieldDef* field_;
    bool expected_;
};

// ALL of the child rules, or ANY one of them
class CompositeRule : public Rule {
public:
    enum Mode { ALL, ANY };
    CompositeRule(const std::string& label, Mode mode, std::vector<std::unique_ptr<Rule> > children);
    RuleResult evaluate(const UserProfile& profile) const override;

private:
    Mode mode_;
    std::vector<std::unique_ptr<Rule> > children_;
};

#endif
