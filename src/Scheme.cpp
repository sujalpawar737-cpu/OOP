#include "Scheme.h"

#include <algorithm>

#include "Utils.h"

int Scheme::liveCount_ = 0;

Scheme::Scheme(const std::string& id, const std::string& name, const std::string& link)
    : Entity(id, name), Linkable(id, name, link), Verifiable(id, name),
      shortName_(name), selectionBased_(false), listBased_(false) {
    if (link.rfind("https://", 0) != 0) {
        throw DataError("Scheme " + id + ": the official link must start with https://");
    }
    liveCount_++;
}

Scheme::~Scheme() {
    liveCount_--;
}

int Scheme::liveCount() {
    return liveCount_;
}

void Scheme::setShortName(const std::string& text) { shortName_ = text; }
void Scheme::setMinistry(const std::string& text) { ministry_ = text; }
void Scheme::setDescription(const std::string& text) { description_ = text; }
void Scheme::setBenefitText(const std::string& text) { benefitText_ = text; }
void Scheme::setNotes(const std::string& text) { notes_ = text; }
void Scheme::setHowToApply(const std::string& text) { howToApply_ = text; }

void Scheme::setApplyLink(const std::string& url) {
    if (!url.empty() && url.rfind("https://", 0) != 0) {
        throw DataError("Scheme " + id_ + ": the apply link must start with https://");
    }
    applyLink_ = url;
}

void Scheme::addDomain(const std::string& domainId) { domains_.push_back(domainId); }
void Scheme::addDocument(const std::string& document) { documents_.push_back(document); }

void Scheme::addRule(std::unique_ptr<Rule> rule) {
    rules_.push_back(std::move(rule));
}

void Scheme::setBenefit(std::unique_ptr<Benefit> benefit) {
    benefit_ = std::move(benefit);
}

bool Scheme::inDomain(const std::string& domainId) const {
    return std::find(domains_.begin(), domains_.end(), domainId) != domains_.end();
}

std::string Scheme::toJson() const {
    std::vector<std::string> criteria;
    for (size_t i = 0; i < rules_.size(); i++) {
        criteria.push_back(rules_[i]->label());
    }

    JsonObject json;
    json.add("id", id_).add("name", name_).add("short", shortName_);
    json.add("ministry", ministry_).add("description", description_);
    json.add("benefit_text", benefitText_).add("link", link_);
    json.add("apply", applyLink_).add("how", howToApply_);
    json.add("notes", notes_).add("status_note", statusNote_).add("last_checked", lastChecked_);
    json.addRaw("documents", jsonStrings(documents_));
    json.addRaw("domains", jsonStrings(domains_));
    json.addRaw("criteria", jsonStrings(criteria));
    json.add("selection_based", selectionBased_).add("list_based", listBased_);
    return json.str();
}

std::ostream& operator<<(std::ostream& out, const Scheme& scheme) {
    out << scheme.id_ << "  " << scheme.shortName_ << "  [" << scheme.benefit_->kind() << " benefit]";
    return out;
}
