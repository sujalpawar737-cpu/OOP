#include "Utils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>

#include "Base.h"

namespace util {

std::string trim(const std::string& text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::string toLower(std::string text) {
    for (size_t i = 0; i < text.size(); i++) {
        text[i] = std::tolower(static_cast<unsigned char>(text[i]));
    }
    return text;
}

// splits on one character, trims every piece and drops empty ones
std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string piece;
    std::istringstream stream(text);
    while (std::getline(stream, piece, separator)) {
        piece = trim(piece);
        if (!piece.empty()) {
            parts.push_back(piece);
        }
    }
    return parts;
}

std::vector<std::string> splitWords(const std::string& text) {
    std::vector<std::string> words;
    std::istringstream stream(text);
    std::string word;
    while (stream >> word) {
        words.push_back(word);
    }
    return words;
}

double toNumber(const std::string& text, const std::string& what) {
    try {
        size_t used = 0;
        double value = std::stod(text, &used);
        if (used != text.size()) {
            throw DataError("Bad number '" + text + "' for " + what);
        }
        return value;
    } catch (const std::invalid_argument&) {
        throw DataError("Bad number '" + text + "' for " + what);
    } catch (const std::out_of_range&) {
        throw DataError("Number too large '" + text + "' for " + what);
    }
}

std::string indian(long long n) {
    bool negative = n < 0;
    std::string digits = std::to_string(negative ? -n : n);
    if (digits.size() > 3) {
        std::string lastThree = digits.substr(digits.size() - 3);
        std::string rest = digits.substr(0, digits.size() - 3);
        std::string result = "";
        while (rest.size() > 2) {
            result = "," + rest.substr(rest.size() - 2) + result;
            rest = rest.substr(0, rest.size() - 2);
        }
        digits = rest + result + "," + lastThree;
    }
    return negative ? "-" + digits : digits;
}

std::string formatValue(double value, const std::string& unit) {
    std::string text;
    if (std::fabs(value - std::round(value)) < 1e-9) {
        text = indian(static_cast<long long>(std::llround(value)));
    } else {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%.2f", value);
        text = buffer;
    }
    if (unit == "inr") return "\xE2\x82\xB9" + text;   // rupee sign
    if (unit == "years") return text + (value == 1 ? " year" : " years");
    if (unit == "percent") return text + "%";
    if (unit == "ha") return text + " ha";
    return text;
}

std::string jsonQuote(const std::string& text) {
    std::string out = "\"";
    for (size_t i = 0; i < text.size(); i++) {
        unsigned char c = text[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) {
            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
            out += buffer;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

std::string jsonNumber(double value) {
    if (!std::isfinite(value)) {
        return "0";
    }
    char buffer[40];
    if (std::fabs(value - std::round(value)) < 1e-9 && std::fabs(value) < 1e15) {
        std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(std::llround(value)));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.10g", value);
    }
    return buffer;
}

}  // namespace util

// ---------------- JsonObject ----------------

void JsonObject::addKey(const std::string& key) {
    if (!body_.empty()) {
        body_ += ",";
    }
    body_ += util::jsonQuote(key) + ":";
}

JsonObject& JsonObject::add(const std::string& key, const std::string& value) {
    addKey(key);
    body_ += util::jsonQuote(value);
    return *this;
}

JsonObject& JsonObject::add(const std::string& key, const char* value) {
    return add(key, std::string(value));
}

JsonObject& JsonObject::add(const std::string& key, double value) {
    addKey(key);
    body_ += util::jsonNumber(value);
    return *this;
}

JsonObject& JsonObject::add(const std::string& key, int value) {
    return add(key, static_cast<double>(value));
}

JsonObject& JsonObject::add(const std::string& key, bool value) {
    addKey(key);
    body_ += value ? "true" : "false";
    return *this;
}

JsonObject& JsonObject::addRaw(const std::string& key, const std::string& json) {
    addKey(key);
    body_ += json;
    return *this;
}

std::string JsonObject::str() const {
    return "{" + body_ + "}";
}

std::string jsonStrings(const std::vector<std::string>& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) {
            out += ",";
        }
        out += util::jsonQuote(items[i]);
    }
    return out + "]";
}
