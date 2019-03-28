#include "store/mortystore/replica.h"

namespace mortystore {

SGStore::SGStore() {
}

SGStore::SGStore(const SGStore &store) : values(store.values) {
}

SGStore::~SGStore() {
}

void SGStore::Write(uint64_t tid, const std::string &key,
    const std::string &value) {
  values[key].v = value;
  values[key].mrw = tid;
  values[key].mrr.clear();
}

void SGStore::Read(uint64_t tid, const std::string &key, std::string &value) {
  auto itr = values.find(key);
  if (itr != values.end()) {
    itr->second.mrr.push_back(tid);
    value = itr->second.v;
  }
}

Replica::Replica(const transport::Configuration &config, int idx,
    Transport *transport) : replication::Replica(config, idx, transport,
      nullptr) {
}

Replica::~Replica() {
}

void Replica::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data) {
  proto::UnloggedRequestMessage unloggedRequest;

  if (type == unloggedRequest.GetTypeName()) {
    unloggedRequest.ParseFromString(data);
    HandleUnloggedRequest(remote, unloggedRequest);
  }
}

void Replica::Load(const std::string &key, const std::string &value,
    const Timestamp timestamp) {
}

void Replica::HandleUnloggedRequest(const TransportAddress &remote,
    const proto::UnloggedRequestMessage &msg) {
  proto::Request request;
  proto::Reply reply;

  request.ParseFromString(msg.req().op());

  switch (request.op()) {
    case proto::Request::GET:
      HandleGetMessage(remote, request.txnid(), request.get());
      break;
    case proto::Request::PUT:
      HandlePutMessage(remote, request.txnid(), request.put());
      break;
    default:
      Panic("Unrecognized UnloggedRequest %d.", request.op());
  }
}

void Replica::HandleGetMessage(const TransportAddress &remote, uint64_t tid,
    const proto::GetMessage &get) {
  AddActiveTransaction(tid);

  std::string value;
  do {
    //Branch *branch = GenerateBranch(tid); // using current permutation stored
    // in activeTidsSorted

    //branch->store.Read(tid, get.key(), value);

    // one option is we generate all possible DGs with adjacency matrices
    //    for each DG, we must ensure:
    //       1. acyclic
    //       2. for each pair of nodes, there is either a path from A to B
    //          or a path from B to A
    //             - we should be able to "check" this while computing all
    //             paths between pairs in Floyd Warshall or some variant
    //
    // another option is we use the algorithm outlined in the Algorithm v2 doc
    // I am concerned that it is hard to analyze the algorithm because it is
    // not written in clean graph theoretic terms. This may be problematic for
    // proving (or even sketching a proof) that our protocol is guaranteed to
    // commit on at least one branch.

  } while (std::next_permutation(activeTidsSorted.begin(),
        activeTidsSorted.end()));
  // for each possible DAG of active and committed transactions where tid is
  //  a sink
}

void Replica::HandlePutMessage(const TransportAddress &remote, uint64_t tid,
    const proto::PutMessage &put) {
  AddActiveTransaction(tid);

 // for each possible DAG of active and committed transactions where tid is
  //  a sink

}

void Replica::AddActiveTransaction(uint64_t tid) {
  if (activeTxns.find(tid) == activeTxns.end()) {
    activeTxns.insert(std::make_pair(tid, nullptr));
    auto insertItr = std::upper_bound(activeTidsSorted.begin(),
        activeTidsSorted.end(), tid);
    activeTidsSorted.insert(insertItr, tid);
  }
}

} // namespace mortystore
