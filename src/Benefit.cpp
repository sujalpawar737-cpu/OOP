#include "Benefit.h"

#include <algorithm>
#include <cmath>

#include "Utils.h"

// ---------------- BenefitValue ----------------

BenefitValue::BenefitValue()
    : annual(0), oneTime(0), years(1), cover(0), futurePension(0), needsInput(false) {}

double BenefitValue::moneyOver(int horizonYears) const {
    return oneTime + annual * std::min(years, horizonYears);
}

std::string BenefitValue::toJson(int horizonYears) const {
    JsonObject json;
    json.add("annual", annual).add("one_time", oneTime).add("years", years);
    json.add("cover", cover).add("future_pension_monthly", futurePension);
    json.add("in_kind", inKind).add("basis", basis).add("needs_input", needsInput);
    json.add("money_over_horizon", moneyOver(horizonYears));
    return json.str();
}

BenefitValue& BenefitValue::operator+=(const BenefitValue& other) {
    annual += other.annual;
    oneTime += other.oneTime;
    cover += other.cover;
    futurePension += other.futurePension;
    if (other.annual > 0 && other.years > years) {
        years = other.years;
    }
    if (!other.inKind.empty()) {
        inKind += (inKind.empty() ? "" : "; ") + other.inKind;
    }
    return *this;
}

BenefitValue operator+(BenefitValue left, const BenefitValue& right) {
    left += right;
    return left;
}

// ---------------- FixedBenefit / ServiceBenefit ----------------

FixedBenefit::FixedBenefit(double annual, double oneTime, int years, double cover,
                           double futurePension, const std::string& basis) {
    value_.annual = annual;
    value_.oneTime = oneTime;
    value_.years = years;
    value_.cover = cover;
    value_.futurePension = futurePension;
    value_.basis = basis;
}

BenefitValue FixedBenefit::evaluate(const UserProfile&) const {
    return value_;
}

ServiceBenefit::ServiceBenefit(const std::string& text)
    : FixedBenefit(0, 0, 1, 0, 0, "Non-monetary benefit - no rupee value is estimated.") {
    value_.inKind = text;
}

// ---------------- AmountBenefit ----------------

AmountBenefit::AmountBenefit(const std::string& field, bool annual, int years, const std::string& basis)
    : field_(field), annual_(annual), years_(years), basis_(basis) {}

BenefitValue AmountBenefit::missingInput(const std::string& hint) const {
    BenefitValue v;
    v.basis = basis_ + " (" + hint + ")";
    v.needsInput = true;
    return v;
}

BenefitValue AmountBenefit::withAmount(double amount) const {
    BenefitValue v;
    v.basis = basis_;
    if (annual_) {
        v.annual = amount;
        v.years = years_;
    } else {
        v.oneTime = amount;
    }
    return v;
}

// ---------------- PerUnitBenefit ----------------

PerUnitBenefit::PerUnitBenefit(const std::string& field, double rate, double capUnits,
                               bool annual, int years, const std::string& basis)
    : AmountBenefit(field, annual, years, basis), rate_(rate), capUnits_(capUnits) {}

BenefitValue PerUnitBenefit::evaluate(const UserProfile& profile) const {
    double units = 0;
    if (!profile.getNumber(field_, units)) {
        return missingInput("Enter the required quantity to estimate this.");
    }
    return withAmount(rate_ * std::min(units, capUnits_));
}

// ---------------- PercentBenefit ----------------

PercentBenefit::PercentBenefit(const std::string& field, double percent, double cap,
                               bool annual, int years, const std::string& basis)
    : AmountBenefit(field, annual, years, basis), percent_(percent), cap_(cap) {}

BenefitValue PercentBenefit::evaluate(const UserProfile& profile) const {
    double base = 0;
    if (!profile.getNumber(field_, base) || base <= 0) {
        return missingInput("Enter the amount to estimate this.");
    }
    return withAmount(std::min(base * percent_ / 100.0, cap_));
}

// ---------------- InterestBenefit ----------------

InterestBenefit::Params::Params()
    : fixedLoan(0), cap(NO_CAP), percent(0), years(1), extraYears(0),
      factor(1.0), recurring(false), emiMonths(0), loanRate(0) {}

BenefitValue InterestBenefit::evaluate(const UserProfile& profile) const {
    const Params& q = params_;
    BenefitValue v;
    v.basis = q.basis;

    double loan = q.fixedLoan;
    if (!q.field.empty()) {
        if (!profile.getNumber(q.field, loan) || loan <= 0) {
            v.needsInput = true;
            v.basis += " (Enter the loan amount to estimate this.)";
            return v;
        }
    }
    loan = std::min(loan, q.cap);

    // monthly EMI loan: subsidy on the balance that is still outstanding each month
    if (q.emiMonths > 0) {
        double r = q.loanRate / 1200.0;
        double n = q.emiMonths;
        double emi = (r > 0) ? loan * r / (1 - std::pow(1 + r, -n)) : loan / n;
        double balance = loan;
        double subsidy = 0;
        for (int month = 0; month < q.emiMonths; month++) {
            subsidy += balance * q.percent / 1200.0;
            balance = balance * (1 + r) - emi;
        }
        v.oneTime = std::round(subsidy);
        return v;
    }

    // prompt-repayment incentive that comes every year
    if (q.recurring) {
        v.annual = loan * q.percent / 100.0;
        v.years = 99;
        return v;
    }

    double years = q.years;
    if (!q.yearsField.empty()) {
        if (!profile.getNumber(q.yearsField, years)) {
            v.needsInput = true;
            v.basis += " (Enter the course duration to estimate this.)";
            return v;
        }
    }
    years += q.extraYears;
    v.oneTime = std::round(loan * q.percent / 100.0 * years * q.factor);
    return v;
}

// ---------------- TieredBenefit ----------------

TieredBenefit::TieredBenefit(std::vector<Tier> tiers, std::unique_ptr<Benefit> fallback)
    : tiers_(std::move(tiers)), fallback_(std::move(fallback)) {}

BenefitValue TieredBenefit::evaluate(const UserProfile& profile) const {
    for (size_t i = 0; i < tiers_.size(); i++) {
        bool allPass = true;
        for (size_t k = 0; k < tiers_[i].when.size(); k++) {
            if (tiers_[i].when[k]->evaluate(profile).outcome != Outcome::Pass) {
                allPass = false;
                break;
            }
        }
        if (allPass) {
            return tiers_[i].benefit->evaluate(profile);
        }
    }
    return fallback_->evaluate(profile);
}

// ---------------- CompositeBenefit ----------------

CompositeBenefit::CompositeBenefit(std::vector<std::unique_ptr<Benefit> > parts, const std::string& basis)
    : parts_(std::move(parts)), basis_(basis) {}

BenefitValue CompositeBenefit::evaluate(const UserProfile& profile) const {
    BenefitValue total;
    total.basis = basis_;
    for (size_t i = 0; i < parts_.size(); i++) {
        BenefitValue part = parts_[i]->evaluate(profile);
        total += part;
        if (part.needsInput && part.annual == 0 && part.oneTime == 0) {
            total.basis += " [" + part.basis + "]";
        }
    }
    return total;
}
