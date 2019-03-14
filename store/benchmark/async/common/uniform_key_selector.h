#ifndef UNIFORM_KEY_SELECTOR_H
#define UNIFORM_KEY_SELECTOR_H

#include "store/benchmark/async/common/key_selector.h"

class UniformKeySelector : public KeySelector {
 public:
  UniformKeySelector(const std::vector<std::string> &keys);
  virtual ~UniformKeySelector();

  virtual int GetKey();

};

#endif /* UNIFORM_KEY_SELECTOR_H */
