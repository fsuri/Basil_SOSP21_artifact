#include "store/mortystore/replica_client.h"

#include "lib/message.h"
#include "replication/common/request.pb.h"

namespace mortystore {

ReplicaClient::ReplicaClient(const transport::Configuration &config,
    Transport *transport, uint64_t clientid) : Client(config, transport,
      clientid) {
}

ReplicaClient::~ReplicaClient() {
}

void ReplicaClient::Invoke(const std::string &request,
    continuation_t continuation,
    error_continuation_t error_continuation) {
  Panic("Not implemented.");
}

void ReplicaClient::InvokeUnlogged(int replicaIdx, const std::string &request,
    continuation_t continuation,
    error_continuation_t error_continuation, uint32_t timeout) {
  uint64_t reqId = ++lastReqId;
  auto timer = std::unique_ptr<Timeout>(new Timeout(transport, timeout,
      [this, reqId]() { UnloggedRequestTimeoutCallback(reqId); }));

  PendingUnloggedRequest *req = new PendingUnloggedRequest(request, reqId,
      continuation, error_continuation, std::move(timer));

  proto::UnloggedRequestMessage reqMsg;
  reqMsg.mutable_req()->set_op(request);
  reqMsg.mutable_req()->set_clientid(clientid);
  reqMsg.mutable_req()->set_clientreqid(reqId);

  if (transport->SendMessageToReplica(this, replicaIdx, reqMsg)) {
    req->timer->Start();
    pendingReqs[reqId] = req;
  } else {
    Warning("Could not send unlogged request to replica");
    delete req;
  }
}

void ReplicaClient::ReceiveMessage(const TransportAddress &remote,
    const std::string &type, const std::string &data) {
  proto::UnloggedReplyMessage unloggedReply;

  if (type == unloggedReply.GetTypeName()) {
    unloggedReply.ParseFromString(data);
    HandleUnloggedReply(remote, unloggedReply);
  } else {
    Client::ReceiveMessage(remote, type, data);
  }
}

void ReplicaClient::MarkComplete() {
  for (auto itr = pendingReqs.begin(); itr != pendingReqs.end(); ++itr) {
    delete itr->second;
    pendingReqs.erase(itr);
  }
}

void ReplicaClient::HandleUnloggedReply(const TransportAddress &remote,
      const proto::UnloggedReplyMessage &reply) {
  uint64_t reqId = reply.clientreqid();
  auto itr = pendingReqs.find(reqId);
  if (itr == pendingReqs.end()) {
    Debug("Received reply for %lu when no request was pending", reqId);
    return;
  }

  PendingRequest *req = itr->second;
  // timer might already be stopped if this is > first reply
  req->timer->Stop();
  // invoke application callback
  req->continuation(req->request, reply.reply());
}

void ReplicaClient::UnloggedRequestTimeoutCallback(const uint64_t reqId) {
  Debug("Unlogged request %lu timed out.", reqId);
}

} // namespace mortystore
