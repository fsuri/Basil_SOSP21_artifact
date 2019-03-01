#ifndef _SPEC_CLIENT_H_
#define _SPEC_CLIENT_H_

#include "lib/udptransport.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/asyncclient.h"
#include "store/mortystore/shardclient.h"

#include <set>
#include <string>
#include <thread>
#include <vector>

namespace mortystore {

class SpecClient : public AsyncClient {
 public:
  SpecClient(const std::string configPath, int nShards,
	    int closestReplica);
  virtual ~SpecClient();

  // Begin a transaction.
  void Begin();

  // Get the value corresponding to key.
  void Get(const std::string &key, get_callback cb);

  // Set the value for the given key.
  void Put(const std::string &key, const std::string &value, put_callback cb);

  // Commit all Get(s) and Put(s) since Begin().
  void Commit(commit_callback cb);
    
  // Abort all Get(s) and Put(s) since Begin().
  void Abort(abort_callback cb);

  // Returns statistics (vector of integers) about most recent transaction.
  std::vector<int> Stats();

 private:
  // Unique ID for this client.
  uint64_t client_id;

  // Ongoing transaction ID.
  uint64_t t_id;

  // Number of shards.
  uint64_t nshards;

  // Number of retries for current transaction.
  long retries;

  // List of participants in the ongoing transaction.
  std::set<int> participants;

  // Transport used by IR client proxies.
  UDPTransport transport;

  std::vector<ShardClient *> sclients;

};

} // namespace mortystore

#endif /* _SPEC_CLIENT_H_ */
