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
#include "store/benchmark/async/retwis/get_timeline.h"

namespace retwis {

GetTimeline::GetTimeline(KeySelector *keySelector, std::mt19937 &rand)
    : RetwisTransaction(keySelector, 1 + rand() % 10, rand) {
}

GetTimeline::~GetTimeline() {
}

Operation GetTimeline::GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues) {
  Debug("GET_TIMELINE %lu %lu", outstandingOpCount, finishedOpCount);
  std::string value;
  if (outstandingOpCount < GetNumKeys()) {
    return Get(GetKey(outstandingOpCount));
  } else if (outstandingOpCount == finishedOpCount) {
    return Commit();
  } else {
    return Wait();
  }
}

} // namespace retwis
