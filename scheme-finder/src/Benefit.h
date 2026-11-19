// Benefit.h - how much a scheme is worth to a citizen.
// Schemes pay out in different ways, so every style is its own subclass of Benefit
// and each one overrides evaluate().
#ifndef BENEFIT_H
#define BENEFIT_H

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "Rule.h"
#include "UserProfile.h"

const double NO_CAP = std::numeric_limits<double>::infinity();

// the result of valuing one scheme (or a whole combination) for one citizen
class BenefitValue {
public:
    BenefitValue();

    double annual;         // yearly cash / subsidy in rupees
    double oneTime;        // one-time cash / subsidy in rupees
    int years;             // how many years the yearly part lasts (99 = while the scheme runs)
    double cover;          // insurance cover - protection, not cash
    double futurePension;  // pension per month, payable later
    std::string inKind;    // non-money benefit, e.g. free training
    std::string basis;     // how the number was worked out
    bool needsInput;       // true when a missing answer stopped us from working it out

    double moneyOver(int horizonYears) const;
    std::string toJson(int horizonYears) const;

    BenefitValue& operator+=(const BenefitValue& other);
};

BenefitValue operator+(BenefitValue left, const BenefitValue& right);

// abstract base class
class Benefit {
public:
    virtual ~Benefit() {}
    virtual BenefitValue evaluate(const UserProfile& profile) const = 0;
    virtual std::string kind() const = 0;
};

// amounts written in the scheme itself, e.g. PM-KISAN pays Rs 6,000 a year
class FixedBenefit : public Benefit {
public:
    FixedBenefit(double annual, double oneTime, int years, double cover,
                 double futurePension, const std::string& basis);
    BenefitValue evaluate(const UserProfile& profile) const override;
    std::string kind() const override { return "fixed"; }

protected:
    BenefitValue value_;
};

// free services: no rupee value is estimated
class ServiceBenefit : public FixedBenefit {
public:
    explicit ServiceBenefit(const std::string& text);
    std::string kind() const override { return "service"; }
};

// common part of benefits that depend on a number the citizen typed in
class AmountBenefit : public Benefit {
public:
    AmountBenefit(const std::string& field, bool annual, int years, const std::string& basis);

protected:
    BenefitValue missingInput(const std::string& hint) const;
    BenefitValue withAmount(double amount) const;

    std::string field_;
    bool annual_;
    int years_;
    std::string basis_;
};

// rate x quantity (for example hectares of land), with an upper limit
class PerUnitBenefit : public AmountBenefit {
public:
    PerUnitBenefit(const std::string& field, double rate, double capUnits,
                   bool annual, int years, const std::string& basis);
    BenefitValue evaluate(const UserProfile& profile) const override;
    std::string kind() const override { return "per unit"; }

private:
    double rate_;
    double capUnits_;
};

// a percentage of a cost the citizen entered (project cost, machinery cost), with a cap
class PercentBenefit : public AmountBenefit {
public:
    PercentBenefit(const std::string& field, double percent, double cap,
                   bool annual, int years, const std::string& basis);
    BenefitValue evaluate(const UserProfile& profile) const override;
    std::string kind() const override { return "percent"; }

private:
    double percent_;
    double cap_;
};

// interest subsidy on a loan
class InterestBenefit : public Benefit {
public:
    struct Params {
        std::string field;       // answer holding the loan amount ("" = use fixedLoan)
        double fixedLoan;
        double cap;
        double percent;
        double years;
        std::string yearsField;  // optional: take the years from an answer
        double extraYears;       // e.g. 1 year moratorium after the course
        double factor;
        bool recurring;          // paid again every year
        int emiMonths;           // > 0 means a monthly EMI loan
        double loanRate;         // yearly loan interest assumed for the EMI case
        std::string basis;
        Params();
    };

    explicit InterestBenefit(const Params& params) : params_(params) {}
    BenefitValue evaluate(const UserProfile& profile) const override;
    std::string kind() const override { return "interest"; }

private:
    Params params_;
};

// a list of (conditions -> benefit); the first tier whose conditions all pass is used
class TieredBenefit : public Benefit {
public:
    struct Tier {
        std::vector<std::unique_ptr<Rule> > when;
        std::unique_ptr<Benefit> benefit;
    };

    TieredBenefit(std::vector<Tier> tiers, std::unique_ptr<Benefit> fallback);
    BenefitValue evaluate(const UserProfile& profile) const override;
    std::string kind() const override { return "tiered"; }

private:
    std::vector<Tier> tiers_;
    std::unique_ptr<Benefit> fallback_;
};

// the sum of several parts, e.g. allowance + fee reimbursement
class CompositeBenefit : public Benefit {
public:
    CompositeBenefit(std::vector<std::unique_ptr<Benefit> > parts, const std::string& basis);
    BenefitValue evaluate(const UserProfile& profile) const override;
    std::string kind() const override { return "composite"; }

private:
    std::vector<std::unique_ptr<Benefit> > parts_;
    std::string basis_;
};

#endif
