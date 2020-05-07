#ifndef PARTITIONER_H
#define PARTITIONER_H

#include <functional>
#include <set>
#include <string>
#include <vector>

enum Partitioner {
  DEFAULT = 0,
  WAREHOUSE,
};

typedef std::function<uint64_t(const std::string &, uint64_t, int,
    const std::vector<int> &)> partitioner;

extern partitioner default_partitioner;
extern partitioner warehouse_partitioner;

partitioner warehouse_district_partitioner(uint64_t num_warehouses);

#endif /* PARTITIONER_H */
