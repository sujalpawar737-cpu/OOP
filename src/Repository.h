// Repository.h - holds everything loaded from data/: the form, all schemes, the
// restrictions between schemes and the site information.
// The data is checked while loading, so a typo stops the server at start-up
// instead of giving wrong results later.
#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <map>
#include <string>
#include <vector>

#include "Base.h"
#include "Combination.h"
#include "Form.h"
#include "Scheme.h"

class SchemeRepository {
public:
    explicit SchemeRepository(const std::string& dataFolder);

    const FormModel& form() const { return form_; }
    const ConflictRegistry& conflicts() const { return conflicts_; }
    const Scheme* find(const std::string& id) const { return schemes_.find(id); }
    std::vector<const Scheme*> inDomain(const std::string& domainId) const;
    std::vector<const Scheme*> allSchemes() const;
    std::string meta(const std::string& key) const;

    // text printed in the terminal when the server starts
    std::string summary() const;

private:
    FormModel form_;
    Registry<Scheme> schemes_;
    ConflictRegistry conflicts_;
    std::map<std::string, std::string> meta_;
};

#endif
