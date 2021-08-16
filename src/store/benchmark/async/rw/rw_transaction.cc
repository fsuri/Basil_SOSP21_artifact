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
#include "store/benchmark/async/rw/rw_transaction.h"

namespace rw {

RWTransaction::RWTransaction(KeySelector *keySelector, int numOps,
    std::mt19937 &rand) : keySelector(keySelector), numOps(numOps) {
  for (int i = 0; i < numOps; ++i) {
    uint64_t key;
    if (i % 2 == 0) {
      key = keySelector->GetKey(rand);
    } else {
      key = keyIdxs[i - 1];
    }
    keyIdxs.push_back(key);
  }
}

RWTransaction::~RWTransaction() {
}

Operation RWTransaction::GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
    std::map<std::string, std::string> readValues) {
  if (outstandingOpCount < GetNumOps()) {
    //std::cerr << "outstanding: " << outstandingOpCount << "; finished: " << finishedOpCount << "num ops: " << GetNumOps() << std::endl;
    if(finishedOpCount != outstandingOpCount){
      return Wait();
    }
    else if (outstandingOpCount % 2 == 0) {
      //std::cerr << "read: " << GetKey(finishedOpCount) << std::endl;
      return Get(GetKey(finishedOpCount));
    } else  {
      //std::cerr << "write: " << GetKey(finishedOpCount) << std::endl;
      auto strValueItr = readValues.find(GetKey(finishedOpCount));
      UW_ASSERT(strValueItr != readValues.end());
      std::string strValue = strValueItr->second;
      std::string writeValue;
      if (strValue.length() == 0) {
        writeValue = std::string(350, '\0'); //make a longer string
      } else {
        uint64_t intValue = 0;
        for (int i = 0; i < 4; ++i) {
          intValue = intValue | (static_cast<uint64_t>(strValue[i]) << ((3 - i) * 8));
        }
        intValue++;
        for (int i = 0; i < 4; ++i) {
          writeValue += static_cast<char>((intValue >> (3 - i) * 8) & 0xFF);
        }
      }
      return Put(GetKey(finishedOpCount), writeValue);
    }
  } else if (finishedOpCount == GetNumOps()) {
    return Commit();
  } else {
    return Wait();
  }
}

} // namespace rw
