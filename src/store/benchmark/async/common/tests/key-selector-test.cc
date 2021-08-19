/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#include <gtest/gtest.h>

#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "store/benchmark/async/common/uniform_key_selector.h"
#include "store/benchmark/async/common/zipf_key_selector.h"

void PrintBuckets(const std::vector<uint64_t> &counts, size_t m, size_t k) {
  uint64_t bucket = 0;
  std::vector<uint64_t> buckets;
  uint64_t max = 0;
  for (size_t i = 1; i < counts.size(); ++i) {
    bucket += counts[i];
    if (i % m == m - 1) {
      if (bucket > max) {
        max = bucket;
      }
      buckets.push_back(bucket);
      bucket = 0;
    }
  }

  for (size_t i = 0; i < buckets.size(); ++i) {
    std::cout << "|";
    for (size_t j = 0; j < k * static_cast<double>(buckets[i]) / max; ++j) {
      std::cout << "=";
    }
    std::cout << std::endl;
  }
}

TEST(UniformKeySelector, CorrectDistribution) {
  size_t k = 100;
  std::vector<std::string> keys;
  for (size_t i = 0; i < k; ++i) {
    keys.push_back(std::to_string(i));
  }
  UniformKeySelector uks(keys);
  std::mt19937 rand;

  std::vector<uint64_t> counts(keys.size(), 0UL);
  size_t n = 1000 * keys.size();
  for (size_t i = 0; i < n; ++i) {
    counts[uks.GetKey(rand)]++;
  }

  PrintBuckets(counts, 10, 20);

  double chisq = 0.0;
  double alpha = 0.05;
  size_t df = k - 1;
  for (size_t i = 0; i < counts.size(); ++i) {
    double expected = n / counts.size();
    chisq += (counts[i] - expected) * (counts[i] - expected) / expected;
  }
  double chisqcritical = 123.225;
  EXPECT_TRUE(chisq < chisqcritical);
}

TEST(ZipfKeySelector, CorrectUniformDistribution) {
  size_t k = 100;
  std::vector<std::string> keys;
  for (size_t i = 0; i < k; ++i) {
    keys.push_back(std::to_string(i));
  }
  ZipfKeySelector uks(keys, 0.0);
  std::mt19937 rand;

  std::vector<uint64_t> counts(keys.size(), 0UL);
  size_t n = 1000 * keys.size();
  for (size_t i = 0; i < n; ++i) {
    counts[uks.GetKey(rand)]++;
  }

  PrintBuckets(counts, 10, 20);

  double chisq = 0.0;
  double alpha = 0.05;
  size_t df = k - 1;
  for (size_t i = 0; i < counts.size(); ++i) {
    double expected = n / counts.size();
    chisq += (counts[i] - expected) * (counts[i] - expected) / expected;
  }
  double chisqcritical = 123.225;
  EXPECT_TRUE(chisq < chisqcritical);
}


