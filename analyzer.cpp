#include "analyzer.h"
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <algorithm>

using namespace std;

static inline int parseHour(const string& timeStr) {
    if (timeStr.length() < 5) return -1;

    char tensDigit = timeStr[0];
    char onesDigit = timeStr[1];

    if (tensDigit < '0' || tensDigit > '9' ||
        onesDigit < '0' || onesDigit > '9') return -1;

    if (timeStr[2] != ':') return -1;

    int hour = (tensDigit - '0') * 10 + (onesDigit - '0');
    if (hour < 0 || hour > 23) return -1;

    return hour;
}

static inline void trimInPlace(string& text) {
    size_t start = 0;
    while (start < text.size()) {
        char ch = text[start];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
        ++start;
    }

    size_t end = text.size();
    while (end > start) {
        char ch = text[end - 1];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
        --end;
    }

    if (start == 0 && end == text.size()) return;
    text = text.substr(start, end - start);
}

void TripAnalyzer::processLine(const string& line) {
    int commaCount = 0;
    for (char ch : line) if (ch == ',') commaCount++;
    if (commaCount < 5) return;

    size_t comma1 = line.find(',');
    if (comma1 == string::npos) return;

    size_t comma2 = line.find(',', comma1 + 1);
    if (comma2 == string::npos) return;

    size_t comma3 = line.find(',', comma2 + 1);
    if (comma3 == string::npos) return;

    size_t comma4 = line.find(',', comma3 + 1);

    string pickupZone = line.substr(comma1 + 1, comma2 - comma1 - 1);
    trimInPlace(pickupZone);
    if (pickupZone.empty()) return;

    string pickupTimestamp;
    if (comma4 != string::npos)
        pickupTimestamp = line.substr(comma3 + 1, comma4 - comma3 - 1);
    else
        pickupTimestamp = line.substr(comma3 + 1);

    trimInPlace(pickupTimestamp);
    if (pickupTimestamp.empty()) return;

    size_t spacePos = pickupTimestamp.find(' ');
    if (spacePos == string::npos || spacePos + 3 > pickupTimestamp.length()) return;

    string pickupTime = pickupTimestamp.substr(spacePos + 1);
    trimInPlace(pickupTime);

    int pickupHour = parseHour(pickupTime);
    if (pickupHour < 0 || pickupHour > 23) return;

    zoneCounts[pickupZone]++;

    auto [zoneIt, inserted] =
        slotCounts.try_emplace(pickupZone, array<long long, 24>{});
    zoneIt->second[pickupHour]++;
}

void TripAnalyzer::ingestFile(const string& csvPath) {
    ifstream file(csvPath);
    if (!file.is_open()) return;

    zoneCounts.reserve(60000);
    slotCounts.reserve(60000);

    string line;
    bool firstLine = true;

    while (getline(file, line)) {
        if (firstLine) {
            firstLine = false;
            if (line.find("TripID") != string::npos) continue;
        }
        processLine(line);
    }
}



vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    vector<ZoneCount> results;
    results.reserve(zoneCounts.size());

    for (auto zoneIt = zoneCounts.begin(); zoneIt != zoneCounts.end(); ++zoneIt) {
        results.push_back({ zoneIt->first, zoneIt->second });
    }

    sort(results.begin(), results.end(),
        [](const ZoneCount& a, const ZoneCount& b) {
            if (a.count != b.count) return a.count > b.count;
            return a.zone < b.zone;
        });

    if ((int)results.size() > k) results.resize(k);
    return results;
}

vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    vector<SlotCount> results;

    for (auto slotIt = slotCounts.begin(); slotIt != slotCounts.end(); ++slotIt) {
        const string& zone = slotIt->first;
        const array<long long, 24>& hours = slotIt->second;

        for (int hourIt = 0; hourIt < 24; hourIt++) {
            long long cnt = hours[hourIt];
            if (cnt > 0)
                results.push_back({ zone, hourIt, cnt });
        }
    }

    sort(results.begin(), results.end(),
        [](const SlotCount& a, const SlotCount& b) {
            if (a.count != b.count) return a.count > b.count;
            if (a.zone != b.zone) return a.zone < b.zone;
            return a.hour < b.hour;
        });

    if ((int)results.size() > k) results.resize(k);
    return results;
}

