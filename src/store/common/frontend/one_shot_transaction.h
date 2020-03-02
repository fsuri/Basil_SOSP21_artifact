#ifndef ONE_SHOT_TRANSACTION_H
#define ONE_SHOT_TRANSACTION_H

#include <map>
#include <set>
#include <string>

class OneShotTransaction {
 public:
  OneShotTransaction();
  virtual ~OneShotTransaction();

  void AddRead(const std::string &key);
  void AddWrite(const std::string &key, const std::string &value);

  inline const std::set<std::string> &GetReadSet() const { return read_set; }
  inline const std::map<std::string, std::string> &GetWriteSet() const {
    return write_set;
  }

 private:
  std::set<std::string> read_set;
  std::map<std::string, std::string> write_set;
};
#endif /* ONE_SHOT_TRANSACTION_H */
