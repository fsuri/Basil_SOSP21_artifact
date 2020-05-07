#include "store/common/partitioner.h"

#include "lib/message.h"

partitioner default_partitioner = [](const std::string &key, uint64_t nshards,
    int group, const std::vector<int> &txnGroups) {
  uint64_t hash = 5381;
  const char* str = key.c_str();
  for (unsigned int i = 0; i < key.length(); i++) {
    hash = ((hash << 5) + hash) + (uint64_t)str[i];
  }
  return (hash % nshards);
};

partitioner warehouse_partitioner = [](const std::string &key, uint64_t nshards,
    int group, const std::vector<int> &txnGroups) {
  // keys format is defined in tpcc_utils.cc
  // bytes 1 through 4 (0-indexed) contain the warehouse id for each table except
  // for the item table (which has no warehouse ids associated with rows).
  uint32_t w_id = *reinterpret_cast<const uint32_t*>(key.c_str() + 1);
  return w_id % nshards;
};


partitioner warehouse_district_partitioner_dist_items(uint64_t num_warehouses) {
  return [num_warehouses](const std::string &key, uint64_t nshards, int group,
      const std::vector<int> &txnGroups) {
    uint32_t w_id = *reinterpret_cast<const uint32_t*>(key.c_str() + 1);
    uint32_t d_id = 0;
    if (key.length() >= 9) {
      d_id = *reinterpret_cast<const uint32_t*>(key.c_str() + 5);
    }
    return ((w_id * num_warehouses) + d_id) % nshards;
  };
}

partitioner warehouse_district_partitioner(uint64_t num_warehouses) {
  return [num_warehouses](const std::string &key, uint64_t nshards, int group,
      const std::vector<int> &txnGroups) {
    switch (key[0]) {
      case 0:  // WAREHOUSE
      {
        uint32_t w_id = *reinterpret_cast<const uint32_t*>(key.c_str() + 1);
        return (w_id * num_warehouses) % nshards;
      }
      case 7:  // ITEM
      {
        if (group == -1) {
          if (txnGroups.size() > 0) {
            return static_cast<uint64_t>(txnGroups[0]);
          } else {
            return 0UL;
          }
        } else {
          return static_cast<uint64_t>(group);
        }
        uint32_t w_id = *reinterpret_cast<const uint32_t*>(key.c_str() + 1);
        return (w_id * num_warehouses) % nshards;
      }
      case 8:  // STOCK
      case 1:  // DISTRICT
      case 2:  // CUSTOMER
      case 3:  // HISTORY
      case 4:  // NEW_ORDER
      case 5:  // ORDER
      case 6:  // ORDER_LINE
      case 9:  // CUSTOMER_BY_NAME
      case 10: // ORDER_BY_CUSTOMER
      case 11: // EARLIEST_NEW_ORDER
      {
        uint32_t w_id = *reinterpret_cast<const uint32_t*>(key.c_str() + 1);
        uint32_t d_id = *reinterpret_cast<const uint32_t*>(key.c_str() + 5);
        return ((w_id * num_warehouses) + d_id) % nshards;
      }
      default:
        return 0UL;
    }
  };
}

