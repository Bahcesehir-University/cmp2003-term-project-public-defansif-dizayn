#include "analyzer.h"
#include <fstream>
#include <algorithm>
#include <cstddef>   

static inline int parseHour(const std::string& timeStr) {
    if (timeStr.length() < 5) return -1;   // "HH:MM"

    char tensDigit = timeStr[0];
    char onesDigit = timeStr[1];

    if (tensDigit < '0' || tensDigit > '9' ||
        onesDigit < '0' || onesDigit > '9')
        return -1;

    if (timeStr[2] != ':') return -1;

    int hour = (tensDigit - '0') * 10 + (onesDigit - '0');
    if (hour < 0 || hour > 23) return -1;

    return hour;
}

static inline void trimInPlace(std::string& text) {
    std::size_t start = 0;
    while (start < text.size()) {
        char ch = text[start];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
        ++start;
    }

    std::size_t end = text.size();
    while (end > start) {
        char ch = text[end - 1];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
        --end;
    }

    if (start == 0 && end == text.size()) return;
    text = text.substr(start, end - start);
}

void TripAnalyzer::processLine(const std::string& line) {
    int commaCount = 0;
    for (char ch : line) if (ch == ',') commaCount++;
    if (commaCount < 5) return;

    std::size_t comma1 = line.find(',');
    if (comma1 == std::string::npos) return;

    std::size_t comma2 = line.find(',', comma1 + 1);
    if (comma2 == std::string::npos) return;

    std::size_t comma3 = line.find(',', comma2 + 1);
    if (comma3 == std::string::npos) return;

    std::size_t comma4 = line.find(',', comma3 + 1);

    std::string pickupZone = line.substr(comma1 + 1, comma2 - comma1 - 1);
    trimInPlace(pickupZone);
    if (pickupZone.empty()) return;

    std::string pickupTimestamp;
    if (comma4 != std::string::npos)
        pickupTimestamp = line.substr(comma3 + 1, comma4 - comma3 - 1);
    else
        pickupTimestamp = line.substr(comma3 + 1);

    trimInPlace(pickupTimestamp);
    if (pickupTimestamp.empty()) return;

    std::size_t spacePos = pickupTimestamp.find(' ');
    if (spacePos == std::string::npos || spacePos + 3 > pickupTimestamp.length()) return;

    std::string pickupTime = pickupTimestamp.substr(spacePos + 1);
    trimInPlace(pickupTime);

    int pickupHour = parseHour(pickupTime);
    if (pickupHour < 0 || pickupHour > 23) return;

    zoneCounts[pickupZone]++;

 
    auto it = slotCounts.find(pickupZone);
    if (it == slotCounts.end()) {
        std::array<long long, 24> arr{};            
        auto ins = slotCounts.emplace(pickupZone, arr);
        it = ins.first;
    }
    it->second[pickupHour]++;
}

void TripAnalyzer::ingestFile(const std::string& csvPath) {
    std::ifstream file(csvPath.c_str());
    if (!file.is_open()) return;

    zoneCounts.reserve(60000);
    slotCounts.reserve(60000);

    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (firstLine) {
            firstLine = false;
            if (line.find("TripID") != std::string::npos) continue;
        }
        processLine(line);
    }
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    std::vector<ZoneCount> results;
    results.reserve(zoneCounts.size());

    for (auto it = zoneCounts.begin(); it != zoneCounts.end(); ++it) {
        results.push_back(ZoneCount{ it->first, it->second });
    }

    std::sort(results.begin(), results.end(),
        [](const ZoneCount& a, const ZoneCount& b) {
            if (a.count != b.count) return a.count > b.count;
            return a.zone < b.zone;
        });

    if ((int)results.size() > k) results.resize(k);
    return results;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    std::vector<SlotCount> results;

    for (auto zit = slotCounts.begin(); zit != slotCounts.end(); ++zit) {
        const std::string& zone = zit->first;
        const std::array<long long, 24>& hours = zit->second;

        for (int h = 0; h < 24; ++h) {
            long long cnt = hours[h];
            if (cnt > 0) results.push_back(SlotCount{ zone, h, cnt });
        }
    }

    std::sort(results.begin(), results.end(),
        [](const SlotCount& a, const SlotCount& b) {
            if (a.count != b.count) return a.count > b.count;
            if (a.zone != b.zone)   return a.zone < b.zone;
            return a.hour < b.hour;
        });

    if ((int)results.size() > k) results.resize(k);
    return results;
}
