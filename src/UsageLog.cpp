#include "UsageLog.h"

#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <queue>

#include "Utils.h"

void UsageLog::add(const std::string& domain, int eligible, int potential, int notEligible) {
    std::lock_guard<std::mutex> guard(lock_);   // localtime() is not thread-safe either

    UsageRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    std::strncpy(rec.domain, domain.c_str(), sizeof(rec.domain) - 1);

    std::time_t now = std::time(nullptr);
    std::strftime(rec.when, sizeof(rec.when), "%Y-%m-%d %H:%M", std::localtime(&now));
    rec.eligible = eligible;
    rec.potential = potential;
    rec.notEligible = notEligible;

    std::ofstream out(path_.c_str(), std::ios::binary | std::ios::app);
    if (!out) {
        return;   // the log is only a bonus feature, so a failure must not stop the check
    }
    out.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
}

std::vector<UsageRecord> UsageLog::readAll() const {
    std::vector<UsageRecord> records;
    std::ifstream in(path_.c_str(), std::ios::binary);
    UsageRecord rec;
    while (in.read(reinterpret_cast<char*>(&rec), sizeof(rec))) {
        rec.domain[sizeof(rec.domain) - 1] = '\0';
        rec.when[sizeof(rec.when) - 1] = '\0';
        records.push_back(rec);
    }
    return records;
}

std::string UsageLog::statsJson() {
    std::lock_guard<std::mutex> guard(lock_);
    std::vector<UsageRecord> records = readAll();

    std::map<std::string, int> perDomain;
    std::queue<UsageRecord> latest;   // keeps only the last 5
    for (size_t i = 0; i < records.size(); i++) {
        perDomain[records[i].domain]++;
        latest.push(records[i]);
        if (latest.size() > 5) {
            latest.pop();
        }
    }

    std::string domainList = "[";
    bool first = true;
    for (auto it = perDomain.begin(); it != perDomain.end(); ++it) {
        JsonObject d;
        d.add("domain", it->first).add("checks", it->second);
        domainList += (first ? "" : ",") + d.str();
        first = false;
    }
    domainList += "]";

    std::string recentList = "[";
    first = true;
    while (!latest.empty()) {
        UsageRecord r = latest.front();
        latest.pop();
        JsonObject o;
        o.add("domain", r.domain).add("when", r.when);
        o.add("eligible", r.eligible).add("potential", r.potential).add("not_eligible", r.notEligible);
        recentList += (first ? "" : ",") + o.str();
        first = false;
    }
    recentList += "]";

    JsonObject json;
    json.add("total", static_cast<int>(records.size()));
    json.addRaw("by_domain", domainList).addRaw("recent", recentList);
    return json.str();
}
