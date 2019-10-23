#ifndef MORTY_COMMON_H
#define MORTY_COMMON_H

#include "store/mortystore/morty-proto.pb.h"

bool operator==(const mortystore::proto::Branch &b1,
    const mortystore::proto::Branch &b2);

bool operator==(const mortystore::proto::Transaction &t1,
    const mortystore::proto::Transaction &t2);

namespace mortystore {

struct BranchHasher {
	size_t operator() (const proto::Branch &b) const;
};

struct BranchComparer {
  bool operator() (const proto::Branch &b1, const proto::Branch &b2) const;
};

void PrintBranch(const proto::Branch &branch);


} // namespace mortystore
#endif /* MORTY_COMMON_H */
