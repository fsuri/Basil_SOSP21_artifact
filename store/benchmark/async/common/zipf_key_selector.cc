#include "store/benchmark/async/common/zipf_key_selector.h"

#include <cmath>

ZipfKeySelector::ZipfKeySelector(const std::vector<std::string> &keys,
    double alpha) : KeySelector(keys), alpha(alpha),
    dist(std::numeric_limits<double>::min(), 1.0) {
  zipf = new double[GetNumKeys()];

  double c = 0.0;
  for (size_t i = 1; i <= GetNumKeys(); i++) {
    c = c + (1.0 / std::pow(static_cast<double>(i), alpha));
  }
  c = 1.0 / c;

  double sum = 0.0;
  for (size_t i = 1; i <= GetNumKeys(); i++) {
    sum += (c / std::pow(static_cast<double>(i), alpha));
    zipf[i-1] = sum;
  }
}

ZipfKeySelector::~ZipfKeySelector() {
  delete [] zipf;
}

int ZipfKeySelector::GetKey(std::mt19937 &rand) {
  double random = dist(rand);

  // binary search to find key;
  int l = 0, r = GetNumKeys(), mid;
  while (l < r) {
    mid = (l + r) / 2;
    if (random > zipf[mid]) {
      l = mid + 1;
    } else if (random < zipf[mid]) {
      r = mid - 1;
    } else {
      break;
    }
  }
  return mid;
}
