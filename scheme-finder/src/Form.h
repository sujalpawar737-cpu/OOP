// Form.h - the questions on the web form (FieldDef), the five domains (DomainDef)
// and FormModel, which checks the answers and builds a UserProfile
#ifndef FORM_H
#define FORM_H

#include <map>
#include <string>
#include <vector>

#include "Base.h"
#include "UserProfile.h"

struct Option {
    std::string value;
    std::string label;
};

// one question: a number, a choice from a list, or a yes/no
class FieldDef : public Entity, public JsonSerializable {
public:
    FieldDef(const std::string& id, const std::string& label, const std::string& type);

    // setters return *this so they can be chained
    FieldDef& setUnit(const std::string& unit);
    FieldDef& setHelp(const std::string& help);
    FieldDef& setRequired(bool required);
    FieldDef& setMin(double min);
    FieldDef& setMax(double max);
    FieldDef& addOption(const std::string& value, const std::string& label);

    const std::string& label() const { return name_; }
    const std::string& type() const { return type_; }
    const std::string& unit() const { return unit_; }
    bool required() const { return required_; }

    bool hasOption(const std::string& value) const;
    std::string optionLabel(const std::string& value) const;

    // checks one raw answer and stores it in the profile; returns false and sets error if invalid
    bool readAnswer(const std::string& raw, UserProfile& profile, std::string& error) const;

    std::string toJson() const override;

private:
    std::string type_;
    std::string unit_;
    std::string help_;
    bool required_;
    bool hasMin_;
    bool hasMax_;
    double min_;
    double max_;
    std::vector<Option> options_;
};

class DomainDef : public Entity {
public:
    DomainDef(const std::string& id, const std::string& name, const std::string& description);

    void addFieldId(const std::string& fieldId);
    const std::string& description() const { return description_; }
    const std::vector<std::string>& fieldIds() const { return fieldIds_; }

private:
    std::string description_;
    std::vector<std::string> fieldIds_;
};

class FormModel {
public:
    void addField(std::unique_ptr<FieldDef> field);
    void addDomain(std::unique_ptr<DomainDef> domain);

    const FieldDef& field(const std::string& id) const;      // throws DataError if unknown
    const DomainDef& domain(const std::string& id) const;    // throws InputError if unknown
    bool hasDomain(const std::string& id) const;
    std::vector<std::string> domainIds() const;

    // answers: field id -> text typed by the user. Throws InputError listing every problem.
    UserProfile buildProfile(const std::string& domainId,
                             const std::map<std::string, std::string>& answers) const;

private:
    Registry<FieldDef> fields_;
    Registry<DomainDef> domains_;
};

#endif
