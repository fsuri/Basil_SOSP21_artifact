/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
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
#include "store/benchmark/async/common/zipf_key_selector.h"

#include <cmath>
#include <iostream>

ZipfKeySelector::ZipfKeySelector(const std::vector<std::string> &keys,
    double zipfianconstant) :
    ZipfKeySelector(keys, zipfianconstant, zetastatic(keys.size(),
        zipfianconstant)) {
}

ZipfKeySelector::ZipfKeySelector(const std::vector<std::string> &keys,
    double zipfianconstant, double zetan) :
    KeySelector(keys), items(keys.size()), base(0),
    zipfianconstant(zipfianconstant), theta(zipfianconstant),
    zeta2theta(zeta(2, theta)), alpha(1.0 / (1.0 - theta)), zetan(zetan),
    countforzeta(items) {
  eta = (1 - std::pow(2.0 / items, 1 - theta)) / (1 - zeta2theta / zetan);
}

int ZipfKeySelector::GetKey(std::mt19937 &rand) {
  return nextLong(items, rand);
}

double ZipfKeySelector::zeta(uint64_t n, double thetaVal) {
  countforzeta = n;
  return zetastatic(n, thetaVal);
}

double ZipfKeySelector::zetastatic(uint64_t n, double theta) {
  return zetastatic(0, n, theta, 0);
}

double ZipfKeySelector::zeta(uint64_t st, uint64_t n, double thetaVal, double initialsum) {
  countforzeta = n;
  return zetastatic(st, n, thetaVal, initialsum);
}

double ZipfKeySelector::zetastatic(uint64_t st, uint64_t n, double theta,
    double initialsum) {
  double sum = initialsum;
  for (uint64_t i = st; i < n; i++) {

    sum += 1 / (std::pow(i + 1, theta));
  }

  return sum;
}

uint64_t ZipfKeySelector::nextLong(uint64_t itemcount, std::mt19937 &rand) {
  //from "Quickly Generating Billion-Record Synthetic Databases", Jim Gray et al, SIGMOD 1994

  if (itemcount != countforzeta) {
    std::lock_guard<std::mutex> l(mtx);
    //have to recompute zetan and eta, since they depend on itemcount
    if (itemcount > countforzeta) {
      //we have added more items. can compute zetan incrementally, which is cheaper
      zetan = zeta(countforzeta, itemcount, theta, zetan);
      eta = (1 - std::pow(2.0 / items, 1 - theta)) / (1 - zeta2theta / zetan);
    } else if ((itemcount < countforzeta) && (allowitemcountdecrease)) {
      //have to start over with zetan
      //note : for large itemsets, this is very slow. so don't do it!

      //TODO: can also have a negative incremental computation, e.g. if you decrease the number of items,
      // then just subtract the zeta sequence terms for the items that went away. This would be faster than
      // recomputing from scratch when the number of items decreases

      std::cerr <<"WARNING: Recomputing Zipfian distribtion. This is slow and should be avoided. " <<
          "(itemcount=" << itemcount << " countforzeta=" << countforzeta + ")" << std::endl;

      zetan = zeta(itemcount, theta);
      eta = (1 - std::pow(2.0 / items, 1 - theta)) / (1 - zeta2theta / zetan);
    }
  }

  double u = dist(rand);
  double uz = u * zetan;

  if (uz < 1.0) {
    return base;
  }

  if (uz < 1.0 + std::pow(0.5, theta)) {
    return base + 1;
  }

  uint64_t ret = base + (uint64_t) ((itemcount) * std::pow(eta * u - eta + 1, alpha));
  return ret;
}
