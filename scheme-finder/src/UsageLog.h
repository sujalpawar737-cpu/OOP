// UsageLog.h - keeps a count of how many checks were done, in a binary file.
// Only the domain name and the three result counts are saved - never the answers
// the citizen typed.
#ifndef USAGELOG_H
#define USAGELOG_H

#include <mutex>
#include <string>
#include <vector>

// one fixed-size record written straight to the file
struct UsageRecord {
    char domain[24];
    char when[20];       // "2026-11-14 10:35"
    int eligible;
    int potential;
    int notEligible;
};

class UsageLog {
public:
    explicit UsageLog(const std::string& path) : path_(path) {}

    void add(const std::string& domain, int eligible, int potential, int notEligible);
    std::string statsJson();   // totals per domain and the latest few checks

private:
    std::vector<UsageRecord> readAll() const;

    std::string path_;
    std::mutex lock_;   // the server handles several requests at once
};

#endif
