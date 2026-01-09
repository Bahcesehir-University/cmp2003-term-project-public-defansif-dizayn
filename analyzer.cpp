#include "analyzer.h"
#include <string>
#include <fstream>
#include <unordered_map>
#include <algorithm>

static inline int parseHour(const std::string& dt) {
    if (dt.size() < 16) return -1;
    if (dt[10] != ' ') return -1;
    if (dt[13] != ':') return -1;

    char h1 = dt[11];
    char h2 = dt[12];

    if (h1 < '0' || h1 > '9' || h2 < '0' || h2 > '9') return -1;

    int hour = (h1 - '0') * 10 + (h2 - '0');
    if (hour < 0 || hour > 23) return -1;

    return hour;
}

static inline void trimInPlace(std::string& s) {
    size_t start = 0;
    while (start < s.size()) {
        char c = s[start];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        ++start;
    }

    size_t end = s.size();
    while (end > start) {
        char c = s[end - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        --end;
    }

    if (start == 0 && end == s.size()) return;
    s = s.substr(start, end - start);
}

void TripAnalyzer::ingestFile(const std::string& csvPath) {
    std::ifstream file(csvPath);
    if (!file.is_open()) return;

    std::string line;
    if (!std::getline(file, line)) return;

    zoneCounts.reserve(60000);
    slotCounts.reserve(60000);

    while (std::getline(file, line)) {
        size_t c1 = line.find(',');
        if (c1 == std::string::npos) continue;

        size_t c2 = line.find(',', c1 + 1);
        if (c2 == std::string::npos) continue;

        size_t c3 = line.find(',', c2 + 1);
        if (c3 == std::string::npos) continue;

        size_t c4 = line.find(',', c3 + 1);
        if (c4 == std::string::npos) continue;

        std::string zone = line.substr(c1 + 1, c2 - c1 - 1);
        trimInPlace(zone);
        if (zone.empty()) continue;

        std::string dt = line.substr(c3 + 1, c4 - c3 - 1);
        trimInPlace(dt);
        if (dt.empty()) continue;

        int hour = parseHour(dt);
        if (hour < 0) continue;

        zoneCounts[zone] += 1;
        slotCounts[zone][hour] += 1;
    }
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    std::vector<ZoneCount> result;
    result.reserve(zoneCounts.size());

    for (auto it = zoneCounts.begin(); it != zoneCounts.end(); ++it) {
        result.push_back(ZoneCount{ it->first, it->second });
    }

    std::sort(result.begin(), result.end(),
        [](const ZoneCount& a, const ZoneCount& b) {
            if (a.count != b.count) return a.count > b.count;
            return a.zone < b.zone;
        });

    if ((int)result.size() > k) result.resize(k);
    return result;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    std::vector<SlotCount> result;

    for (auto zit = slotCounts.begin(); zit != slotCounts.end(); ++zit) {
        const std::string& zone = zit->first;
        const std::unordered_map<int, long long>& hours = zit->second;

        for (auto hit = hours.begin(); hit != hours.end(); ++hit) {
            int hour = hit->first;
            long long cnt = hit->second;

            if (cnt <= 0) continue;
            if (hour < 0 || hour > 23) continue;

            result.push_back(SlotCount{ zone, hour, cnt });
        }
    }

    std::sort(result.begin(), result.end(),
        [](const SlotCount& a, const SlotCount& b) {
            if (a.count != b.count) return a.count > b.count;
            if (a.zone != b.zone)   return a.zone < b.zone;
            return a.hour < b.hour;
        });

    if ((int)result.size() > k) result.resize(k);
    return result;
}
