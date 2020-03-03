#ifndef PARTITIONER_H
#define PARTITIONER_H

#include <functional>
#include <string>

enum Partitioner {
  DEFAULT = 0,
  WAREHOUSE,
};

typedef std::function<uint64_t(const std::string &, uint64_t)> partitioner;

extern partitioner default_partitioner;
extern partitioner warehouse_partitioner;

#endif /* PARTITIONER_H */
