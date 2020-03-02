#ifndef ZIPF_KEY_SELECTOR_H
#define ZIPF_KEY_SELECTOR_H

#include "store/benchmark/async/common/key_selector.h"

class ZipfKeySelector : public KeySelector {
 public:
  ZipfKeySelector(const std::vector<std::string> &keys, double alpha);
  virtual ~ZipfKeySelector();

  virtual int GetKey(std::mt19937 &rand) override;

 private:
  double alpha;
  double *zipf;
  std::uniform_real_distribution<double> dist;
};

#endif /* ZIPF_KEY_SELECTOR_H */
