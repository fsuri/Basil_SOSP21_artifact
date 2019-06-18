#ifndef STATS_H
#define STATS_H

#include <mutex>
#include <ostream>
#include <string>
#include <unordered_map>

class Stats {
 public:
  Stats();
  virtual ~Stats();

  void Increment(const std::string &key, int amount);

  void ExportJSON(std::ostream &os);
  void ExportJSON(const std::string &file);
  void Merge(const Stats &other);

 private:
  std::mutex mtx;
  std::unordered_map<std::string, int> statInts;
};
#endif /* STATS_H */
