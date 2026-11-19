// Base.h - exceptions, the JSON interface, Entity and the Registry template
#ifndef BASE_H
#define BASE_H

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// thrown when a data file is missing or has a mistake in it
class DataError : public std::runtime_error {
public:
    explicit DataError(const std::string& message) : std::runtime_error(message) {}
};

// thrown when the answers entered on the form are not valid
class InputError : public std::runtime_error {
public:
    InputError(const std::string& message,
               const std::vector<std::string>& problems = std::vector<std::string>())
        : std::runtime_error(message), problems_(problems) {}

    const std::vector<std::string>& problems() const { return problems_; }

private:
    std::vector<std::string> problems_;
};

// anything that can turn itself into JSON for the web pages
class JsonSerializable {
public:
    virtual ~JsonSerializable() {}
    virtual std::string toJson() const = 0;
};

// something with an id and a name (fields, domains, schemes)
class Entity {
public:
    Entity(const std::string& id, const std::string& name) : id_(id), name_(name) {}
    virtual ~Entity() {}

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }

protected:
    std::string id_;
    std::string name_;
};

// keeps objects in the order they were added and lets us find them by id
template <typename T>
class Registry {
public:
    void add(std::unique_ptr<T> item) {
        if (byId_.count(item->id()) > 0) {
            throw DataError("Duplicate id: " + item->id());
        }
        byId_[item->id()] = item.get();
        items_.push_back(std::move(item));
    }

    const T* find(const std::string& id) const {
        auto it = byId_.find(id);
        if (it == byId_.end()) {
            return nullptr;
        }
        return it->second;
    }

    const std::vector<std::unique_ptr<T> >& items() const { return items_; }
    size_t size() const { return items_.size(); }

private:
    std::vector<std::unique_ptr<T> > items_;
    std::map<std::string, T*> byId_;
};

#endif
