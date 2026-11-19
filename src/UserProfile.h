// UserProfile.h - the answers one citizen gave on the form
#ifndef USERPROFILE_H
#define USERPROFILE_H

#include <map>
#include <string>

// A question that was skipped or answered "Not sure" is simply missing here.
// That is how the rules can tell "does not qualify" apart from "cannot tell yet".
class UserProfile {
public:
    void setNumber(const std::string& id, double value);
    void setText(const std::string& id, const std::string& value);
    void setFlag(const std::string& id, bool value);

    // each getter returns false when the answer is missing
    bool getNumber(const std::string& id, double& value) const;
    bool getText(const std::string& id, std::string& value) const;
    bool getFlag(const std::string& id, bool& value) const;

private:
    std::map<std::string, double> numbers_;
    std::map<std::string, std::string> texts_;
    std::map<std::string, bool> flags_;
};

#endif
