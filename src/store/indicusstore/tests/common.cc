#include "store/indicusstore/tests/common.h"

namespace indicusstore {

void GenerateTestConfig(int g, int f, std::stringstream &ss) {
  int n = 5 * f + 1;
  ss << "f " << f << std::endl;
  for (int i = 0; i < g; ++i) {
    ss << "group" << std::endl;
    for (int j = 0; j < n; ++j) {
      ss << "replica localhost:8000" << std::endl;
    }
  }
}

} // namespace indicusstore
