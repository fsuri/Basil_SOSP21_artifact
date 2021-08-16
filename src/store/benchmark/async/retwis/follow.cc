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
#include "store/benchmark/async/retwis/follow.h"

namespace retwis {

Follow::Follow(KeySelector *keySelector, std::mt19937 &rand) :
    RetwisTransaction(keySelector, 2, rand) {
}

Follow::~Follow() {

}

Operation Follow::GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues) {
  Debug("FOLLOW %lu %lu", outstandingOpCount, finishedOpCount);
  if (outstandingOpCount == 0) {
    return Get(GetKey(0));
  } else if (outstandingOpCount == 1) {
    return Put(GetKey(0), GetKey(0));
  } else if (outstandingOpCount == 2) {
    return Get(GetKey(1));
  } else if (outstandingOpCount == 3) {
    return Put(GetKey(1), GetKey(1));
  } else if (outstandingOpCount == finishedOpCount) {
    return Commit();
  } else {
    return Wait();
  }
}

} // namespace retwis
