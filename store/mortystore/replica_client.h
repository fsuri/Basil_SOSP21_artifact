#ifndef REPLICA_CLIENT_H
#define REPLICA_CLIENT_H

#include "replication/common/client.h"
#include "lib/configuration.h"
#include "replication/common/request.pb.h"
#include "store/mortystore/morty-proto.pb.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>

namespace mortystore {

class ReplicaClient : public replication::Client {
 public:
  ReplicaClient(const transport::Configuration &config, Transport *transport,
      uint64_t clientid);
  virtual ~ReplicaClient();

  virtual void Invoke(const std::string &request, continuation_t continuation,
      error_continuation_t error_continuation) override;
  virtual void InvokeUnlogged(int replicaIdx, const std::string &request,
      continuation_t continuation,
      error_continuation_t error_continuation, uint32_t timeout) override;
  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data) override;

  // for now, ReplicaClient assumes it will only invoke requests for a single
  // txn between MarkComplete() calls
  void MarkComplete();

 protected:
  struct PendingRequest {
    std::string request;
    uint64_t clientReqId;
    continuation_t continuation;
    bool continuationInvoked = false;
    std::unique_ptr<Timeout> timer;

    inline PendingRequest(std::string request, uint64_t clientReqId,
        continuation_t continuation, std::unique_ptr<Timeout> timer,
        int quorumSize) : request(request), clientReqId(clientReqId),
        continuation(continuation), timer(std::move(timer)) { }
    virtual ~PendingRequest(){}
  };

  struct PendingUnloggedRequest : public PendingRequest {
      error_continuation_t error_continuation;

      inline PendingUnloggedRequest(std::string request, uint64_t clientReqId,
          continuation_t continuation, error_continuation_t error_continuation,
          std::unique_ptr<Timeout> timer) : PendingRequest(request,
            clientReqId, continuation, std::move(timer), 1),
          error_continuation(error_continuation) { }
  };

  void HandleUnloggedReply(const TransportAddress &remote,
      const proto::UnloggedReplyMessage &reply);
  void UnloggedRequestTimeoutCallback(const uint64_t reqId);

  uint64_t lastReqId;
  std::unordered_map<uint64_t, PendingRequest *> pendingReqs;

};

} // namespace mortystore

#endif  /* REPLICa_CLIENT_H */
