// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#ifndef _JANUS_STORE_H_
#define _JANUS_STORE_H_

#include "lib/assert.h"
#include "lib/message.h"

#include "store/janusstore/transaction.h"

#include <unordered_map>

namespace janusstore {

class Store
{
public:
    Store();
    ~Store();

    int Get(uint64_t id, const std::string &key, std::string> &value);
    int Put(uint64_t id, const std::string &key, const std::string &value);

private:
    // unversioned data store (for a particular shard, i would think)
    std::unordered_map<std::string, std::string> store;

    // TODO in janus, this store doesnt seem very interesting because
    // commit logic for a transaction is in the replica (server.h) that wraps
    // this store
};

} // namespace janusstore

#endif /* _JANUS_STORE_H_ */
