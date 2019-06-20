#include "store/common/partitioner.h"

#include "lib/message.h"

partitioner default_partitioner = [](const std::string &key, uint64_t nshards){
  uint64_t hash = 5381;
  const char* str = key.c_str();
  for (unsigned int i = 0; i < key.length(); i++) {
    hash = ((hash << 5) + hash) + (uint64_t)str[i];
  }
  return (hash % nshards);
};

partitioner warehouse_partitioner = [](const std::string &key, uint64_t nshards) {
  // keys format is defined in tpcc_utils.cc
  // bytes 1 through 4 (0-indexed) contain the warehouse id for each table except
  // for the item table (which has no warehouse ids associated with rows).
  uint32_t w_id = *reinterpret_cast<const uint32_t*>(key.c_str() + 1);
  return w_id % nshards;
};
