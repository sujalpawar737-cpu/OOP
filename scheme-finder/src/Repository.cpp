#include "Repository.h"

#include <set>
#include <sstream>

#include "Loader.h"

SchemeRepository::SchemeRepository(const std::string& dataFolder) {
    DataLoader loader(dataFolder);
    loader.loadForm(form_);
    loader.loadSchemes(form_, schemes_);
    loader.loadConflicts(conflicts_);
    meta_ = loader.loadMeta();

    std::set<std::string> ids;
    for (size_t i = 0; i < schemes_.items().size(); i++) {
        ids.insert(schemes_.items()[i]->id());
    }
    conflicts_.checkIds(ids);
}

std::vector<const Scheme*> SchemeRepository::inDomain(const std::string& domainId) const {
    std::vector<const Scheme*> result;
    for (size_t i = 0; i < schemes_.items().size(); i++) {
        if (schemes_.items()[i]->inDomain(domainId)) {
            result.push_back(schemes_.items()[i].get());
        }
    }
    return result;
}

// every scheme once (a scheme can belong to two domains)
std::vector<const Scheme*> SchemeRepository::allSchemes() const {
    std::vector<const Scheme*> result;
    for (size_t i = 0; i < schemes_.items().size(); i++) {
        result.push_back(schemes_.items()[i].get());
    }
    return result;
}

std::string SchemeRepository::meta(const std::string& key) const {
    auto it = meta_.find(key);
    return it == meta_.end() ? "" : it->second;
}

std::string SchemeRepository::summary() const {
    std::ostringstream out;
    std::vector<std::string> domains = form_.domainIds();
    for (size_t i = 0; i < domains.size(); i++) {
        size_t count = inDomain(domains[i]).size();
        out << "  " << domains[i] << ": " << count << " schemes";
        if (count < 10 || count > 15) {
            out << "  (warning: expected 10 to 15)";
        }
        out << "\n";
    }
    out << "  " << Scheme::liveCount() << " schemes, " << conflicts_.size() << " restriction records loaded";
    return out.str();
}
