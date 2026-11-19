#include "Form.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "Utils.h"

// ---------------- FieldDef ----------------

FieldDef::FieldDef(const std::string& id, const std::string& label, const std::string& type)
    : Entity(id, label), type_(type), required_(false),
      hasMin_(false), hasMax_(false), min_(0), max_(0) {
    if (type != "number" && type != "select" && type != "bool") {
        throw DataError("Unknown field type '" + type + "' for " + id);
    }
}

FieldDef& FieldDef::setUnit(const std::string& unit) { unit_ = unit; return *this; }
FieldDef& FieldDef::setHelp(const std::string& help) { help_ = help; return *this; }
FieldDef& FieldDef::setRequired(bool required) { required_ = required; return *this; }
FieldDef& FieldDef::setMin(double min) { min_ = min; hasMin_ = true; return *this; }
FieldDef& FieldDef::setMax(double max) { max_ = max; hasMax_ = true; return *this; }

FieldDef& FieldDef::addOption(const std::string& value, const std::string& label) {
    Option o;
    o.value = value;
    o.label = label;
    options_.push_back(o);
    return *this;
}

bool FieldDef::hasOption(const std::string& value) const {
    for (size_t i = 0; i < options_.size(); i++) {
        if (options_[i].value == value) {
            return true;
        }
    }
    return false;
}

std::string FieldDef::optionLabel(const std::string& value) const {
    for (size_t i = 0; i < options_.size(); i++) {
        if (options_[i].value == value) {
            return options_[i].label;
        }
    }
    return value;
}

bool FieldDef::readAnswer(const std::string& raw, UserProfile& profile, std::string& error) const {
    if (type_ == "number") {
        std::string text = raw;
        text.erase(std::remove(text.begin(), text.end(), ','), text.end());   // allow 1,20,000
        double number = 0;
        try {
            size_t used = 0;
            number = std::stod(text, &used);
            if (used != text.size() || !std::isfinite(number)) {
                throw std::invalid_argument("not a number");
            }
        } catch (const std::exception&) {
            error = name_ + " must be a number.";
            return false;
        }
        if (hasMin_ && number < min_) {
            error = name_ + " must be at least " + util::formatValue(min_, "") + ".";
            return false;
        }
        if (hasMax_ && number > max_) {
            error = name_ + " must be at most " + util::formatValue(max_, "") + ".";
            return false;
        }
        profile.setNumber(id_, number);
        return true;
    }

    if (type_ == "select") {
        if (!hasOption(raw)) {
            error = name_ + ": invalid choice.";
            return false;
        }
        profile.setText(id_, raw);
        return true;
    }

    // yes / no question
    if (raw == "yes" || raw == "true") {
        profile.setFlag(id_, true);
        return true;
    }
    if (raw == "no" || raw == "false") {
        profile.setFlag(id_, false);
        return true;
    }
    error = name_ + ": answer Yes, No or Not sure.";
    return false;
}

std::string FieldDef::toJson() const {
    JsonObject json;
    json.add("id", id_).add("label", name_).add("type", type_);
    json.add("unit", unit_).add("help", help_).add("required", required_);
    if (hasMin_) json.add("min", min_);
    if (hasMax_) json.add("max", max_);
    if (!options_.empty()) {
        std::string list = "[";
        for (size_t i = 0; i < options_.size(); i++) {
            if (i > 0) list += ",";
            JsonObject o;
            o.add("value", options_[i].value).add("label", options_[i].label);
            list += o.str();
        }
        json.addRaw("options", list + "]");
    }
    return json.str();
}

// ---------------- DomainDef ----------------

DomainDef::DomainDef(const std::string& id, const std::string& name, const std::string& description)
    : Entity(id, name), description_(description) {}

void DomainDef::addFieldId(const std::string& fieldId) {
    fieldIds_.push_back(fieldId);
}

// ---------------- FormModel ----------------

void FormModel::addField(std::unique_ptr<FieldDef> field) {
    fields_.add(std::move(field));
}

void FormModel::addDomain(std::unique_ptr<DomainDef> domain) {
    for (size_t i = 0; i < domain->fieldIds().size(); i++) {
        field(domain->fieldIds()[i]);   // throws if the field does not exist
    }
    domains_.add(std::move(domain));
}

const FieldDef& FormModel::field(const std::string& id) const {
    const FieldDef* f = fields_.find(id);
    if (f == nullptr) {
        throw DataError("Unknown field id in data: " + id);
    }
    return *f;
}

const DomainDef& FormModel::domain(const std::string& id) const {
    const DomainDef* d = domains_.find(id);
    if (d == nullptr) {
        throw InputError("Please select a valid domain.");
    }
    return *d;
}

bool FormModel::hasDomain(const std::string& id) const {
    return domains_.find(id) != nullptr;
}

std::vector<std::string> FormModel::domainIds() const {
    std::vector<std::string> ids;
    for (size_t i = 0; i < domains_.items().size(); i++) {
        ids.push_back(domains_.items()[i]->id());
    }
    return ids;
}

UserProfile FormModel::buildProfile(const std::string& domainId,
                                    const std::map<std::string, std::string>& answers) const {
    const DomainDef& dom = domain(domainId);
    UserProfile profile;
    std::vector<std::string> problems;

    for (size_t i = 0; i < dom.fieldIds().size(); i++) {
        const FieldDef& f = field(dom.fieldIds()[i]);
        std::string raw;
        auto it = answers.find(f.id());
        if (it != answers.end()) {
            raw = util::trim(it->second);
        }

        // blank or "unknown" means the person did not answer
        if (raw.empty() || raw == "unknown") {
            if (f.required()) {
                problems.push_back(f.label() + " is required.");
            }
            continue;
        }

        std::string error;
        if (!f.readAnswer(raw, profile, error)) {
            problems.push_back(error);
        }
    }

    if (!problems.empty()) {
        throw InputError("Please correct the highlighted answers.", problems);
    }
    return profile;
}
