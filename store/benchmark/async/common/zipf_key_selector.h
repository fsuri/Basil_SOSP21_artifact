#ifndef UNIFORM_KEY_SELECTOR_H
#define UNIFORM_KEY_SELECTOR_H

#include "store/benchmark/async/common/key_selector.h"

class ZipfKeySelector : public KeySelector {
 public:
  ZipfKeySelector(const std::vector<std::string> &keys, double alpha);
  virtual ~ZipfKeySelector();

  virtual int GetKey();

 private:
  double alpha;
  double *zipf;
};

#endif /* ZIPF_KEY_SELECTOR_H */
