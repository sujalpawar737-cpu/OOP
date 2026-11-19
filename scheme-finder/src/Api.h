// Api.h - connects the web requests to the classes. Every method returns the JSON text
// that the web pages read. Nothing about the citizen is kept between requests.
#ifndef API_H
#define API_H

#include <map>
#include <string>
#include <vector>

#include "Combination.h"
#include "Eligibility.h"
#include "Repository.h"
#include "UsageLog.h"

typedef std::map<std::string, std::string> Params;   // form field name -> value

class ApiController {
public:
    ApiController(const SchemeRepository& repo, UsageLog& log);

    std::string meta() const;
    std::string domains() const;
    std::string directory(const std::string& domain, const std::string& query) const;
    std::string stats();

    // status is set to the HTTP status code (200, 400 or 422)
    std::string evaluate(const Params& params, int& status);
    std::string combine(const Params& params, int& status) const;

private:
    Preferences readPreferences(const Params& params) const;
    std::string errorJson(const InputError& e) const;
    std::string recommendation(const std::vector<const Assessment*>& candidates, const Preferences& pref) const;
    std::string restrictions(const std::vector<const Assessment*>& candidates) const;

    const SchemeRepository& repo_;
    UsageLog& log_;
    EligibilityEngine engine_;
    CombinationEngine combos_;
};

#endif
