#include "store/mortystore/server.h"

namespace mortystore {

Server::Server(const transport::Configuration &config, int idx,
    Transport *transport) : config(config), idx(idx), transport(transport) {
}

Server::~Server() {
}

void Server::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data) {
  proto::Read read;
  proto::Write write;
  proto::Prepare prepare;
  proto::KO ko;
  proto::Commit commit;
  proto::Abort abort;

  if (type == read.GetTypeName()) {
    read.ParseFromString(data);
    HandleRead(remote, read);
  } else if (type == write.GetTypeName()) {
    write.ParseFromString(data);
    HandleWrite(remote, write);
  } else if (type == prepare.GetTypeName()) {
    prepare.ParseFromString(data);
    HandlePrepare(remote, prepare);
  } else if (type == ko.GetTypeName()) {
    ko.ParseFromString(data);
    HandleKO(remote, ko);
  } else if (type == commit.GetTypeName()) {
    commit.ParseFromString(data);
    HandleCommit(remote, commit);
  } else if (type == abort.GetTypeName()) {
    abort.ParseFromString(data);
    HandleAbort(remote, abort);
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}

void Server::Load(const std::string &key, const std::string &value,
    const Timestamp timestamp) {
}

void Server::HandleRead(const TransportAddress &remote, const proto::Read &msg) {
  CacheBranch(msg.branch());

  auto itr = pending_reads.find(msg.key());
  if (itr == pending_reads.end()) {
    pending_reads.insert(std::make_pair(msg.key(), std::unordered_set<uint64_t>()));
  }
  pending_reads[msg.key()].insert(msg.branch().id());

  SendBranchReplies(proto::OperationType::READ, msg.key());
}

void Server::HandleWrite(const TransportAddress &remote, const proto::Write &msg) {
  CacheBranch(msg.branch());

  auto itr = pending_writes.find(msg.key());
  if (itr == pending_writes.end()) {
    pending_writes.insert(std::make_pair(msg.key(), std::unordered_set<uint64_t>()));
  }
  pending_writes[msg.key()].insert(msg.branch().id());

  SendBranchReplies(proto::OperationType::WRITE, msg.key());
}

void Server::HandlePrepare(const TransportAddress &remote, const proto::Prepare &msg) {
  CacheBranch(msg.branch());

  if (!CheckBranch(remote, msg.branch().id())) {
    waiting.insert(msg.branch().id());
  }
}

void Server::HandleKO(const TransportAddress &remote, const proto::KO &msg) {
  auto itr = std::find(prepared.begin(), prepared.end(), msg.branch().id());
  if (itr != prepared.end()) {
    prepared.erase(itr);
    for (; itr != prepared.end(); ++itr) {
      std::vector<uint64_t> prep(prepared.begin(), itr - 1);
      if (!CommitCompatible(*itr, prep)) {
        prepared.erase(itr);
      }
    }
  }
}

void Server::HandleCommit(const TransportAddress &remote, const proto::Commit &msg) {
  committed.push_back(msg.branch().id());
  for (auto itr =  pending_reads.begin(); itr != pending_reads.end(); ++itr) {
    for (auto jtr = itr->second.begin(); jtr != itr->second.end(); ++jtr) {
      if (branches[*jtr]->txn().id() == msg.branch().txn().id()) {
        itr->second.erase(jtr);
      }
    }
  }
  for (auto itr =  pending_writes.begin(); itr != pending_writes.end(); ++itr) {
    for (auto jtr = itr->second.begin(); jtr != itr->second.end(); ++jtr) {
      if (branches[*jtr]->txn().id() == msg.branch().txn().id()) {
        itr->second.erase(jtr);
      }
    }
  }
  
  for (auto itr = waiting.begin(); itr != waiting.end(); ++itr) {
    if (CheckBranch(*txn_coordinators[branches[*itr]->txn().id()],
          *itr)) {
      waiting.erase(itr);
      for (auto shard : branches[*itr]->shards()) {
        transport->SendMessage(this, *shards[shard], msg);
      }
    }
  }
}

void Server::HandleAbort(const TransportAddress &remote, const proto::Abort &msg) {
}

bool Server::CheckBranch(const TransportAddress &addr, uint64_t branch) {
  if (CommitCompatible(branch, prepared) && CommitCompatible(branch, committed)) {
    prepared.push_back(branch);
    proto::PrepareOK reply;
    *reply.mutable_branch() = *branches[branch];
    transport->SendMessage(this, addr, reply);
    return true;
  } else if (!WaitCompatible(branch, committed)) {
    proto::PrepareKO reply;
    *reply.mutable_branch() = *branches[branch];
    transport->SendMessage(this, addr, reply);
    return true;
  } else if (!WaitCompatible(branch, prepared)) {
    proto::PrepareKO reply;
    *reply.mutable_branch() = *branches[branch];
    transport->SendMessage(this, addr, reply);
    return true;
  } else {
    return false;
  }
}

void Server::CacheBranch(const proto::Branch &branch) {
  auto itr = branches.find(branch.id());
  if (itr != branches.end()) {
    delete itr->second; 
  }
  proto::Branch *copy = new proto::Branch(branch);
  branches[branch.id()] = copy;
}

void Server::SendBranchReplies(proto::OperationType type, const std::string &key) {
  std::vector<proto::Branch *> generated_branches;
  GenerateBranches(type, key, generated_branches);
  for (auto branch : generated_branches) {
    const proto::Operation op = branch->txn().ops()[branch->txn().ops().size() - 1];
    if (op.type() == proto::OperationType::READ) {
      std::string val;
      ValueOnBranch(branch, op.key(), val);
      proto::ReadReply reply;
      *reply.mutable_branch() =  *branch;
      reply.set_key(op.key());
      reply.set_value(val);
      transport->SendMessage(this, *txn_coordinators[branch->txn().id()], reply);
    } else {
      proto::WriteReply reply;
      *reply.mutable_branch() = *branch;
      reply.set_key(op.key());
      reply.set_value(op.val());
      transport->SendMessage(this, *txn_coordinators[branch->txn().id()], reply);
    }
  }
}

void Server::GenerateBranches(proto::OperationType type,
    const std::string &key, std::vector<proto::Branch *> new_branches) {
  std::vector<uint64_t> generated_branches;
  std::unordered_set<uint64_t> pending_branches;
  if (type == proto::OperationType::WRITE) {
    pending_branches.insert(pending_writes[key].begin(),
        pending_writes[key].end());
    pending_branches.insert(pending_reads[key].begin(),
        pending_reads[key].end());
  } else {
    pending_branches.insert(pending_writes[key].begin(),
        pending_writes[key].end());
  }
  std::unordered_set<uint64_t> txns;
  for (auto branch : pending_branches) {
    txns.insert(branches[branch]->txn().id()); 
  }
  std::vector<uint64_t> txns_list;
  txns_list.insert(txns_list.end(), txns.begin(), txns.end());

  GenerateBranchesSubsets(pending_branches, txns_list, new_branches);
}

void Server::GenerateBranchesSubsets(const std::unordered_set<uint64_t> &pending_branches,
    const std::vector<uint64_t> &txns, std::vector<proto::Branch *> new_branches,
    std::vector<uint64_t> subset, size_t i) {
  if (i == txns.size()) {
    return;
  }

  GenerateBranchesPermutations(pending_branches, subset, new_branches);

  for (size_t j = i + 1; j < txns.size(); ++j) {
    subset.push_back(txns[j]);

    GenerateBranchesSubsets(pending_branches, txns, new_branches, subset, j);

    subset.pop_back();
  }
}

void Server::GenerateBranchesPermutations(const std::unordered_set<uint64_t> &pending_branches,
    const std::vector<uint64_t> &txns, std::vector<proto::Branch *> new_branches) {
  std::vector<uint64_t> txns_sorted(txns);
  std::sort(txns_sorted.begin(), txns_sorted.end());
  do {
    std::vector<std::vector<uint64_t>> new_seqs;
    new_seqs.push_back(committed);

    for (size_t i = 0; i < txns_sorted.size() - 1; ++i) {
      std::vector<std::vector<uint64_t>> new_seqs1;
      for (size_t j = 0; j < new_seqs.size(); ++j) {
        for (auto branch : pending_branches) {
          if (branches[branch]->txn().id() == txns_sorted[i] &&
              CommitCompatible(branch, new_seqs[j])) {
            std::vector<uint64_t> seq(new_seqs[j]);
            seq.push_back(branch);
            new_seqs1.push_back(seq);
          }
        }
      }
      new_seqs.insert(new_seqs.end(), new_seqs1.begin(), new_seqs1.end());
    }
    for (auto branch : pending_branches) {
      if (branches[branch]->txn().id() == txns_sorted[txns_sorted.size() - 1]) {
        for (auto seq : new_seqs) {
          if (CommitCompatible(branch, seq)) {
            proto::Branch* new_branch = new proto::Branch(*branches[branch]); 
            new_branch->clear_seq();
            for (auto b : seq) {
              proto::Branch *bseq = new_branch->add_seq();
              *bseq = *branches[b];
            }
            new_branches.push_back(new_branch);
          }
        }
      }
    }
  } while (std::next_permutation(txns_sorted.begin(), txns_sorted.end()));
}

bool Server::WaitCompatible(uint64_t branch, const std::vector<uint64_t> &seq) {
  std::vector<uint64_t> seq3;
  std::vector<uint64_t> seq4(seq);
  for (auto b : branches[branch]->seq()) {
    auto itr = std::find(seq4.begin(), seq4.end(), b.id());
    if (itr != seq4.end()) {
      seq4.erase(itr);
    }
    auto itr2 = std::find(seq.begin(), seq.end(), b.id());
    if (itr2 != seq.end()) {
      seq3.push_back(b.id());
    }
  }
  return ValidSubsequence(branches[branch]->txn(), seq3, seq) &&
      NoConflicts(branches[branch]->txn(), seq4);
}

bool Server::CommitCompatible(uint64_t branch,
    const std::vector<uint64_t> &seq) {
  std::vector<uint64_t> seq3;
  std::vector<uint64_t> seq4(seq);
  for (auto b : branches[branch]->seq()) {
    seq3.push_back(b.id());
    auto itr = std::find(seq4.begin(), seq4.end(), b.id());
    if (itr != seq4.end()) {
      seq4.erase(itr);
    }
  }
  return ValidSubsequence(branches[branch]->txn(), seq3, seq) &&
      NoConflicts(branches[branch]->txn(), seq4);
}

bool Server::ValidSubsequence(const proto::Transaction &txn,
      const std::vector<uint64_t> &seq1,
      const std::vector<uint64_t> &seq2) {
  size_t k = 0;
  for (size_t i = 0; i < seq1.size(); ++i) {
    bool found = false;
    for (size_t j =  k + 1; j < seq2.size(); ++j) {
      if (branches[seq1[i]]->txn().id() == branches[seq2[j]]->txn().id()) {
        proto::Transaction txn1(branches[seq2[j]]->txn());
        google::protobuf::RepeatedPtrField<proto::Operation> ops(txn1.ops());
        for (int l = 0; l < branches[seq1[i]]->txn().ops().size(); ++l) {
          const proto::Operation &op2 = branches[seq1[i]]->txn().ops()[l];
          auto itr = std::find_if(ops.begin(), ops.end(),
              [op2](const proto::Operation &op) {
                  return op.type() == op2.type() && op.key() == op2.key() &&
                      op.val() == op2.val();
                });
          if (itr != ops.end()) {
            ops.erase(itr);
          }
        }
        *txn1.mutable_ops() = ops;
        if (TransactionsConflict(txn, txn1)) {
          return false;
        } else {
          k = j;
          found = true;
        }
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

bool Server::NoConflicts(const proto::Transaction &txn,
      const std::vector<uint64_t> &seq) {
  for (size_t i = 0; i < seq.size(); ++i) {
    if (TransactionsConflict(txn, branches[seq[i]]->txn())) {
      return false;
    }
  }
  return true;
}

bool Server::TransactionsConflict(const proto::Transaction &txn1,
      const proto::Transaction &txn2) {
  std::set<std::string> rs1;
  std::set<std::string> ws1;
  for (auto op : txn1.ops()) {
    if (op.type() == proto::OperationType::READ) {
      rs1.insert(op.key());
    } else {
      ws1.insert(op.key());
    }
  }
  std::set<std::string> rs2;
  std::set<std::string> ws2;
  for (auto op : txn2.ops()) {
    if (op.type() == proto::OperationType::READ) {
      rs2.insert(op.key());
    } else {
      ws2.insert(op.key());
    }
  }
  std::vector<std::string> rs1ws2;
  std::vector<std::string> rs2ws1;
  std::vector<std::string> ws1ws2;
  std::set_intersection(rs1.begin(), rs1.end(), ws2.begin(), ws2.end(),
      std::back_inserter(rs1ws2));
  if (rs1ws2.size() > 0) {
    return true;
  }
  std::set_intersection(rs2.begin(), rs2.end(), ws1.begin(), ws1.end(),
      std::back_inserter(rs2ws1));
  if (rs2ws1.size() > 0) {
    return true;
  }
  std::set_intersection(ws1.begin(), ws1.end(), ws2.begin(), ws2.end(),
      std::back_inserter(ws1ws2));
  if (ws1ws2.size() > 0) {
    return true;
  } else {
    return false;
  }
}

void Server::ValueOnBranch(const proto::Branch *branch, const std::string &key,
    std::string &val) {
  if (ValueInTransaction(branch->txn(), key, val)) {
    return;
  }
  ValueOnBranch(&branch->seq()[branch->seq().size() - 1], key, val);
}

bool Server::ValueInTransaction(const proto::Transaction &txn, const std::string &key,
    std::string &val) {
  for (auto itr = txn.ops().rbegin(); itr != txn.ops().rend(); ++itr) {
    if (itr->type() == proto::OperationType::WRITE && itr->key() == key) {
      val = itr->val();
      return true;
    }
  }
  return false;
}

} // namespace mortystore
