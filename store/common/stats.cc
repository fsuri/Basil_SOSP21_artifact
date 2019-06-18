#include "store/common/stats.h"

#include <fstream>

Stats::Stats() {
}

Stats::~Stats() {
}

void Stats::Increment(const std::string &key, int amount) {
  std::lock_guard<std::mutex> lock(mtx);
  statInts[key] += amount;
}

void Stats::ExportJSON(std::ostream &os) {
  std::lock_guard<std::mutex> lock(mtx);
  os << "{" << std::endl;
  for (auto itr = statInts.begin(); itr != statInts.end(); ++itr) {
    os << "    \"" << itr->first << "\": " << itr->second;
    if (std::next(itr) != statInts.end()) {
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
    statInts[s.first] += statInts[s.first];
  }
}
