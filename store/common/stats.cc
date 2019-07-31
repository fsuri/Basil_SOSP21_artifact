#include "store/common/stats.h"

#include <algorithm>
#include <fstream>

Stats::Stats() {
}

Stats::~Stats() {
}

void Stats::Increment(const std::string &key, int amount) {
  std::lock_guard<std::mutex> lock(mtx);
  statInts[key] += amount;
}

void Stats::Add(const std::string &key, int value) {
  std::lock_guard<std::mutex> lock(mtx);
  statLists[key].push_back(value);
}

void Stats::ExportJSON(std::ostream &os) {
  std::lock_guard<std::mutex> lock(mtx);
  os << "{" << std::endl;
  for (auto itr = statInts.begin(); itr != statInts.end(); ++itr) {
    os << "    \"" << itr->first << "\": " << itr->second;
    if (std::next(itr) != statInts.end() || statLists.size() > 0) {
      os << ",";
    }
    os << std::endl;
  }
  for (auto itr = statLists.begin(); itr != statLists.end(); ++itr) {
    os << "    \"" << itr->first << "\": [";
    for (auto jtr = itr->second.begin(); jtr != itr->second.end(); ++jtr) {
      os << *jtr;
      if (std::next(jtr) != itr->second.end()) {
        os << ", ";
      }
    }
    os << "]";
    if (std::next(itr) != statLists.end()) {
      os << ",";
    }
    os << std::endl;
  }
  os << "}" << std::endl;
}

void Stats::ExportJSON(const std::string &file) {
  std::ofstream ofs(file);
  if (!ofs.fail()) {
    ExportJSON(ofs);
    ofs.close();
  }
}

void Stats::Merge(const Stats &other) {
  std::lock_guard<std::mutex> lock(mtx);
  for (auto s : other.statInts) {
    statInts[s.first] += s.second;
  }
  for (auto l : other.statLists) {
    statLists[l.first].insert(statLists[l.first].end(), l.second.begin(),
        l.second.end());
  }
}
