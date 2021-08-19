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
/**
 * Adapted from YCSB Java implementation.
 *
 * Copyright (c) 2010-2016 Yahoo! Inc., 2017 YCSB contributors. All rights reserved.
 * <p>
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You
 * may obtain a copy of the License at
 * <p>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License. See accompanying
 * LICENSE file.
 */

#ifndef ZIPF_KEY_SELECTOR_H
#define ZIPF_KEY_SELECTOR_H

#include <mutex>

#include "store/benchmark/async/common/key_selector.h"

/**
 * A generator of a zipfian distribution. It produces a sequence of items, such that some items are more popular than
 * others, according to a zipfian distribution. When you construct an instance of this class, you specify the number
 * of items in the set to draw from, either by specifying an itemcount (so that the sequence is of items from 0 to
 * itemcount-1) or by specifying a min and a max (so that the sequence is of items from min to max inclusive). After
 * you construct the instance, you can change the number of items by calling nextInt(itemcount) or nextLong(itemcount).
 *
 * Note that the popular items will be clustered together, e.g. item 0 is the most popular, item 1 the second most
 * popular, and so on (or min is the most popular, min+1 the next most popular, etc.) If you don't want this clustering,
 * and instead want the popular items scattered throughout the item space, then use ScrambledZipfKeySelector instead.
 *
 * Be aware: initializing this generator may take a uint64_t time if there are lots of items to choose from (e.g. over a
 * minute for 100 million objects). This is because certain mathematical values need to be computed to properly
 * generate a zipfian skew, and one of those values (zeta) is a sum sequence from 1 to n, where n is the itemcount.
 * Note that if you increase the number of items in the set, we can compute a new zeta incrementally, so it should be
 * fast unless you have added millions of items. However, if you decrease the number of items, we recompute zeta from
 * scratch, so this can take a uint64_t time.
 *
 * The algorithm used here is from "Quickly Generating Billion-Record Synthetic Databases", Jim Gray et al, SIGMOD 1994.
 */
class ZipfKeySelector : public KeySelector {
 public:
  /******************************* Constructors **************************************/

  /**
   * Create a zipfian generator for the specified number of items using the specified zipfian constant.
   *
   * @param keys The keys in the distribution.
   * @param zipfianconstant The zipfian constant to use.
   */
  ZipfKeySelector(const std::vector<std::string> &keys, double zipfianconstant);

  /**
   * Create a zipfian generator for items between min and max (inclusive) for the specified zipfian constant, using
   * the precomputed value of zeta.
   *
   * @param min The smallest integer to generate in the sequence.
   * @param max The largest integer to generate in the sequence.
   * @param zipfianconstant The zipfian constant to use.
   * @param zetan The precomputed zeta constant.
   */
  ZipfKeySelector(const std::vector<std::string> &keys, double zipfianconstant,
      double zetan);


  /**************************************************************************/

  /**
   * Return the next value, skewed by the Zipfian distribution. The 0th item will be the most popular, followed by
   * the 1st, followed by the 2nd, etc. (Or, if min != 0, the min-th item is the most popular, the min+1th item the
   * next most popular, etc.) If you want the popular items scattered throughout the item space, use
   * ScrambledZipfKeySelector instead.
   */
  int GetKey(std::mt19937 &rand) override;
 
  const double ZIPFIAN_CONSTANT = 0.99;

 private:
  /**
   * Compute the zeta constant needed for the distribution. Do this from scratch for a distribution with n items,
   * using the zipfian constant thetaVal. Remember the value of n, so if we change the itemcount, we can recompute zeta.
   *
   * @param n The number of items to compute zeta over.
   * @param thetaVal The zipfian constant.
   */
  double zeta(uint64_t n, double thetaVal);

  /**
   * Compute the zeta constant needed for the distribution. Do this from scratch for a distribution with n items,
   * using the zipfian constant theta. This is a static version of the function which will not remember n.
   * @param n The number of items to compute zeta over.
   * @param theta The zipfian constant.
   */
  static double zetastatic(uint64_t n, double theta);

  /**
   * Compute the zeta constant needed for the distribution. Do this incrementally for a distribution that
   * has n items now but used to have st items. Use the zipfian constant thetaVal. Remember the new value of
   * n so that if we change the itemcount, we'll know to recompute zeta.
   *
   * @param st The number of items used to compute the last initialsum
   * @param n The number of items to compute zeta over.
   * @param thetaVal The zipfian constant.
   * @param initialsum The value of zeta we are computing incrementally from.
   */
  double zeta(uint64_t st, uint64_t n, double thetaVal, double initialsum);

  /**
   * Compute the zeta constant needed for the distribution. Do this incrementally for a distribution that
   * has n items now but used to have st items. Use the zipfian constant theta. Remember the new value of
   * n so that if we change the itemcount, we'll know to recompute zeta.
   * @param st The number of items used to compute the last initialsum
   * @param n The number of items to compute zeta over.
   * @param theta The zipfian constant.
   * @param initialsum The value of zeta we are computing incrementally from.
   */
  static double zetastatic(uint64_t st, uint64_t n, double theta,
      double initialsum);

  /****************************************************************************************/


  /**
   * Generate the next item as a uint64_t.
   *
   * @param itemcount The number of items in the distribution.
   * @return The next item in the sequence.
   */
  uint64_t nextLong(uint64_t itemcount, std::mt19937 &rand);

  /**
   * Number of items.
   */
  const uint64_t  items;

  /**
   * Min item to generate.
   */
  const uint64_t base;

  /**
   * The zipfian constant to use.
   */
  const double zipfianconstant;

  /**
   * Computed parameters for generating the distribution.
   */
  double theta, zeta2theta, alpha, zetan, eta;

  /**
   * The number of items used to compute zetan the last time.
   */
  uint64_t countforzeta;

  /**
   * Flag to prevent problems. If you increase the number of items the zipfian generator is allowed to choose from,
   * this code will incrementally compute a new zeta value for the larger itemcount. However, if you decrease the
   * number of items, the code computes zeta from scratch; this is expensive for large itemsets.
   * Usually this is not intentional; e.g. one thread thinks the number of items is 1001 and calls "nextLong()" with
   * that item count; then another thread who thinks the number of items is 1000 calls nextLong() with itemcount=1000
   * triggering the expensive recomputation. (It is expensive for 100 million items, not really for 1000 items.) Why
   * did the second thread think there were only 1000 items? maybe it read the item count before the first thread
   * incremented it. So this flag allows you to say if you really do want that recomputation. If true, then the code
   * will recompute zeta if the itemcount goes down. If false, the code will assume itemcount only goes up, and never
   * recompute.
   */
  bool allowitemcountdecrease = false;

  std::uniform_real_distribution<double> dist;
  std::mutex mtx;

};

#endif /* ZIPF_KEY_SELECTOR_H */
