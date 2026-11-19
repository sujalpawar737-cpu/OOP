#include "UserProfile.h"

void UserProfile::setNumber(const std::string& id, double value) {
    numbers_[id] = value;
}

void UserProfile::setText(const std::string& id, const std::string& value) {
    texts_[id] = value;
}

void UserProfile::setFlag(const std::string& id, bool value) {
    flags_[id] = value;
}

bool UserProfile::getNumber(const std::string& id, double& value) const {
    auto it = numbers_.find(id);
    if (it == numbers_.end()) {
        return false;
    }
    value = it->second;
    return true;
}

bool UserProfile::getText(const std::string& id, std::string& value) const {
    auto it = texts_.find(id);
    if (it == texts_.end()) {
        return false;
    }
    value = it->second;
    return true;
}

bool UserProfile::getFlag(const std::string& id, bool& value) const {
    auto it = flags_.find(id);
    if (it == flags_.end()) {
        return false;
    }
    value = it->second;
    return true;
}
