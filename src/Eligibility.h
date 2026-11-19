// Eligibility.h - runs every rule of a scheme against a citizen and gives an Assessment.
//   Eligible    - every criterion passes     -> "Eligible based on the information provided"
//   Potential   - an answer is missing, or selection / list inclusion is outside our knowledge
//                                            -> "Potentially eligible; verify with the official authority"
//   NotEligible - at least one criterion fails
// The system never reports guaranteed eligibility.
#ifndef ELIGIBILITY_H
#define ELIGIBILITY_H

#include <string>
#include <vector>

#include "Scheme.h"

enum class Status { Eligible, Potential, NotEligible };

std::string statusCode(Status status);
std::string statusHeadline(Status status);

class Assessment : public JsonSerializable {
public:
    Assessment();

    const Scheme* scheme;
    Status status;
    std::vector<RuleResult> criteria;
    std::vector<std::string> missing;   // criteria we could not check
    std::vector<std::string> failed;    // criteria that failed
    std::vector<std::string> caveats;   // why a fully passing scheme is still only "potential"
    BenefitValue value;
    int horizon;                        // planning period used for the money figures

    bool usable(bool includePotential) const;
    std::string toJson() const override;
};

class EligibilityEngine {
public:
    Assessment assess(const Scheme& scheme, const UserProfile& profile, int horizon) const;
    std::vector<Assessment> assessAll(const std::vector<const Scheme*>& schemes,
                                      const UserProfile& profile, int horizon) const;
};

#endif
