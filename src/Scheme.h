// Scheme.h - one government scheme.
// A scheme is an Entity (id + name) that has an official link and a "last checked" record.
// Entity is a virtual base class, so Scheme gets only one copy of it even though it
// reaches Entity through both Linkable and Verifiable.
#ifndef SCHEME_H
#define SCHEME_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Base.h"
#include "Benefit.h"
#include "Rule.h"

class Linkable : virtual public Entity {
public:
    Linkable(const std::string& id, const std::string& name, const std::string& link)
        : Entity(id, name), link_(link) {}
    const std::string& link() const { return link_; }

protected:
    std::string link_;
};

class Verifiable : virtual public Entity {
public:
    Verifiable(const std::string& id, const std::string& name) : Entity(id, name) {}
    void setLastChecked(const std::string& date) { lastChecked_ = date; }
    void setStatusNote(const std::string& note) { statusNote_ = note; }
    const std::string& lastChecked() const { return lastChecked_; }
    const std::string& statusNote() const { return statusNote_; }

protected:
    std::string lastChecked_;
    std::string statusNote_;
};

class Scheme : public Linkable, public Verifiable, public JsonSerializable {
public:
    Scheme(const std::string& id, const std::string& name, const std::string& link);
    ~Scheme();

    // how many Scheme objects exist right now
    static int liveCount();

    void setShortName(const std::string& text);
    void setMinistry(const std::string& text);
    void setDescription(const std::string& text);
    void setBenefitText(const std::string& text);
    void setNotes(const std::string& text);
    void setHowToApply(const std::string& text);
    void setApplyLink(const std::string& url);   // direct application page (optional)
    void setSelectionBased(bool value) { selectionBased_ = value; }
    void setListBased(bool value) { listBased_ = value; }
    void addDomain(const std::string& domainId);
    void addDocument(const std::string& document);
    void addRule(std::unique_ptr<Rule> rule);
    void setBenefit(std::unique_ptr<Benefit> benefit);

    const std::string& shortName() const { return shortName_; }
    const std::string& ministry() const { return ministry_; }
    const std::string& description() const { return description_; }
    const std::string& benefitText() const { return benefitText_; }
    const std::string& notes() const { return notes_; }
    const std::string& howToApply() const { return howToApply_; }
    const std::string& applyLink() const { return applyLink_; }
    const std::vector<std::string>& domains() const { return domains_; }
    const std::vector<std::unique_ptr<Rule> >& rules() const { return rules_; }
    const Benefit& benefit() const { return *benefit_; }
    bool selectionBased() const { return selectionBased_; }   // competitive or quota based
    bool listBased() const { return listBased_; }             // depends on an official beneficiary list

    bool inDomain(const std::string& domainId) const;
    std::string toJson() const override;

    friend std::ostream& operator<<(std::ostream& out, const Scheme& scheme);

private:
    static int liveCount_;

    std::string shortName_;
    std::string ministry_;
    std::string description_;
    std::string benefitText_;
    std::string notes_;
    std::string howToApply_;
    std::string applyLink_;
    std::vector<std::string> domains_;
    std::vector<std::string> documents_;
    std::vector<std::unique_ptr<Rule> > rules_;
    std::unique_ptr<Benefit> benefit_;
    bool selectionBased_;
    bool listBased_;
};

#endif
