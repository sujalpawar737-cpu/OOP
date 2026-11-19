// Utils.h - small string helpers, number formatting and a JSON builder
#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

namespace util {

std::string trim(const std::string& text);
std::string toLower(std::string text);
std::vector<std::string> split(const std::string& text, char separator);
std::vector<std::string> splitWords(const std::string& text);
double toNumber(const std::string& text, const std::string& what);

std::string indian(long long n);                                  // 1234567 -> 12,34,567
std::string formatValue(double value, const std::string& unit);   // adds Rs / years / % etc.

std::string jsonQuote(const std::string& text);
std::string jsonNumber(double value);

}  // namespace util

// builds one JSON object: {"key":value,...}
// add() is overloaded so the same call works for text, numbers and true/false
class JsonObject {
public:
    JsonObject& add(const std::string& key, const std::string& value);
    JsonObject& add(const std::string& key, const char* value);
    JsonObject& add(const std::string& key, double value);
    JsonObject& add(const std::string& key, int value);
    JsonObject& add(const std::string& key, bool value);
    JsonObject& addRaw(const std::string& key, const std::string& json);  // already-built JSON
    std::string str() const;

private:
    void addKey(const std::string& key);
    std::string body_;
};

std::string jsonStrings(const std::vector<std::string>& items);

// works for any class that has a toJson() method
template <typename T>
std::string jsonArray(const std::vector<T>& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) {
            out += ",";
        }
        out += items[i].toJson();
    }
    return out + "]";
}

#endif
