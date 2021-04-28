#include "store/benchmark/async/rw/rw_transaction.h"

namespace rw {

RWTransaction::RWTransaction(KeySelector *keySelector, int numOps,
    std::mt19937 &rand) : keySelector(keySelector), numOps(numOps) {
  for (int i = 0; i < numOps; ++i) {
    uint64_t key;
    if (i < numOps / 2) {
      key = keySelector->GetKey(rand);
    } else {
      key = keyIdxs[i - numOps / 2];
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
    if (outstandingOpCount < GetNumOps() / 2) {
      //std::cerr << "read: " << GetKey(finishedOpCount) << std::endl;
      return Get(GetKey(outstandingOpCount));
    } else if (finishedOpCount >= GetNumOps() / 2) {
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
      return Put(GetKey(outstandingOpCount), writeValue);
    } else {
      return Wait();
    }
  } else if (finishedOpCount == GetNumOps()) {
    return Commit();
  } else {
    return Wait();
  }
}

} // namespace rw
