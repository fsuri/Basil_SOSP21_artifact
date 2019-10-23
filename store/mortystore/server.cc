#include "store/mortystore/server.h"

#include "store/mortystore/common.h"

#include <google/protobuf/util/message_differencer.h>

namespace mortystore {

Server::Server(const transport::Configuration &config, int groupIdx, int idx,
    Transport *transport) : config(config), idx(idx), transport(transport) {
  transport->Register(this, config, groupIdx, idx);
}

Server::~Server() {
}

void Server::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) {
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
  if (committed_txn_ids.find(msg.branch().txn().id()) != committed_txn_ids.end()) {
    // msg is for already committed txn
    return;
  }

  txn_coordinators[msg.branch().txn().id()] = &remote;

  pending_reads[msg.key()].push_back(msg.branch());

  SendBranchReplies(msg.branch(), proto::OperationType::READ, msg.key());
}

void Server::HandleWrite(const TransportAddress &remote, const proto::Write &msg) {
  if (committed_txn_ids.find(msg.branch().txn().id()) != committed_txn_ids.end()) {
    // msg is for already committed txn
    return;
  }

  std::cerr << "Received write: ";
  PrintBranch(msg.branch());

  txn_coordinators[msg.branch().txn().id()] = &remote;

  pending_writes[msg.key()].push_back(msg.branch());

  SendBranchReplies(msg.branch(), proto::OperationType::WRITE, msg.key());
}

void Server::HandlePrepare(const TransportAddress &remote, const proto::Prepare &msg) {
  if (committed_txn_ids.find(msg.branch().txn().id()) != committed_txn_ids.end()) {
    // msg is for already committed txn
    return;
  }

  if (!CheckBranch(remote, msg.branch())) {
    waiting.push_back(msg.branch());
  }
}

void Server::HandleKO(const TransportAddress &remote, const proto::KO &msg) {
  if (committed_txn_ids.find(msg.branch().txn().id()) != committed_txn_ids.end()) {
    // msg is for already committed txn
    return;
  }

  auto itr = std::find_if(prepared.begin(), prepared.end(), [&](const proto::Transaction &other) {
          return google::protobuf::util::MessageDifferencer::Equals(msg.branch().txn(), other);
        });
  if (itr != prepared.end()) {
    prepared.erase(itr);
    ++itr;
    for (; itr != prepared.end(); ++itr) {
      // TODO: this is probably unsafe, unpreparing all transactions that prepared
      // after this koed txn
      prepared.erase(itr);
    }
  }
}

void Server::HandleCommit(const TransportAddress &remote, const proto::Commit &msg) {
  if (committed_txn_ids.find(msg.branch().txn().id()) != committed_txn_ids.end()) {
    // msg is for already committed txn
    return;
  }

  committed.push_back(msg.branch().txn());
  committed_txn_ids.insert(msg.branch().txn().id());

  for (auto itr = pending_reads.begin(); itr != pending_reads.end(); ++itr) {
    itr->second.erase(std::remove_if(itr->second.begin(), itr->second.end(), [&](const proto::Branch &b) {
        return b.txn().id() == msg.branch().txn().id();
    }), itr->second.end());
  }
  for (auto itr = pending_writes.begin(); itr != pending_writes.end(); ++itr) {
    itr->second.erase(std::remove_if(itr->second.begin(), itr->second.end(), [&](const proto::Branch &b) {
        return b.txn().id() == msg.branch().txn().id();
    }), itr->second.end());
  }
  
  for (auto itr = waiting.begin(); itr != waiting.end(); ) {
    if (CheckBranch(*txn_coordinators[itr->txn().id()],
          *itr)) {
      waiting.erase(itr);
      for (auto shard : itr->shards()) {
        transport->SendMessage(this, *shards[shard], msg);
      }
    } else {
      ++itr;
    }
  }
}

void Server::HandleAbort(const TransportAddress &remote, const proto::Abort &msg) {
}

bool Server::CheckBranch(const TransportAddress &addr, const proto::Branch &branch) {
  if (CommitCompatible(branch, prepared) && CommitCompatible(branch, committed)) {
    prepared.push_back(branch.txn());
    proto::PrepareOK reply;
    *reply.mutable_branch() = branch;
    transport->SendMessage(this, addr, reply);
    return true;
  } else if (!WaitCompatible(branch, committed)) {
    proto::PrepareKO reply;
    *reply.mutable_branch() = branch;
    transport->SendMessage(this, addr, reply);
    return true;
  } else if (!WaitCompatible(branch, prepared)) {
    proto::PrepareKO reply;
    *reply.mutable_branch() = branch;
    transport->SendMessage(this, addr, reply);
    return true;
  } else {
    return false;
  }
}

void Server::SendBranchReplies(const proto::Branch &init,
    proto::OperationType type, const std::string &key) {
  std::vector<proto::Branch> generated_branches;
  GenerateBranches(init, type, key, generated_branches);
  already_generated.insert(already_generated.end(), generated_branches.begin(),
      generated_branches.end());
  for (const proto::Branch &branch : generated_branches) {
    const proto::Operation &op = branch.txn().ops()[branch.txn().ops().size() - 1];
    if (op.type() == proto::OperationType::READ) {
      std::string val;
      ValueOnBranch(branch, op.key(), val);
      proto::ReadReply reply;
      *reply.mutable_branch() =  branch;
      reply.set_key(op.key());
      reply.set_value(val);
      transport->SendMessage(this, *txn_coordinators[branch.txn().id()], reply);
    } else {
      proto::WriteReply reply;
      *reply.mutable_branch() = branch;
      reply.set_key(op.key());
      reply.set_value(op.val());
      transport->SendMessage(this, *txn_coordinators[branch.txn().id()], reply);
    }
  }
}

void Server::GenerateBranches(const proto::Branch &init, proto::OperationType type,
    const std::string &key, std::vector<proto::Branch> &new_branches) {
  std::vector<proto::Branch> generated_branches;
  std::unordered_set<proto::Branch, BranchHasher, BranchComparer> pending_branches;
  pending_branches.insert(init);
  if (type == proto::OperationType::WRITE) {
    pending_branches.insert(pending_writes[key].begin(),
        pending_writes[key].end());
    pending_branches.insert(pending_reads[key].begin(),
        pending_reads[key].end());
  } else {
    pending_branches.insert(pending_writes[key].begin(),
        pending_writes[key].end());
  }
  std::cerr << "Pending branches:" << std::endl;
  std::unordered_set<uint64_t> txns;
  for (const proto::Branch &branch : pending_branches) {
    txns.insert(branch.txn().id()); 
    PrintBranch(branch);
  }
  std::vector<uint64_t> txns_list;
  txns_list.insert(txns_list.end(), txns.begin(), txns.end());

  GenerateBranchesSubsets(pending_branches, txns_list, new_branches);
}

void Server::GenerateBranchesSubsets(const std::unordered_set<proto::Branch, BranchHasher, BranchComparer> &pending_branches,
    const std::vector<uint64_t> &txns, std::vector<proto::Branch> &new_branches,
    std::vector<uint64_t> subset, int64_t i) {
  if (subset.size() > 0) {
    GenerateBranchesPermutations(pending_branches, subset, new_branches);
  }

  for (size_t j = i + 1; j < txns.size(); ++j) {
    subset.push_back(txns[j]);

    GenerateBranchesSubsets(pending_branches, txns, new_branches, subset, j);

    subset.pop_back();
  }
}

void Server::GenerateBranchesPermutations(const std::unordered_set<proto::Branch, BranchHasher, BranchComparer> &pending_branches,
    const std::vector<uint64_t> &txns, std::vector<proto::Branch> &new_branches) {
  std::vector<uint64_t> txns_sorted(txns);
  std::sort(txns_sorted.begin(), txns_sorted.end());
  do {
    std::cerr << "Permutation: [";
    for (size_t i = 0; i < txns_sorted.size(); ++i) {
      std::cerr << txns_sorted[i];
      if (i < txns_sorted.size() - 1) {
        std::cerr << ", ";
      }
    }
    std::cerr << "]" << std::endl;
    std::vector<std::vector<proto::Transaction>> new_seqs;
    new_seqs.push_back(committed);

    for (size_t i = 0; i < txns_sorted.size() - 1; ++i) {
      std::vector<std::vector<proto::Transaction>> new_seqs1;
      for (size_t j = 0; j < new_seqs.size(); ++j) {
        for (const proto::Branch &branch : pending_branches) {
          if (branch.txn().id() == txns_sorted[i]) {
            std::cerr << "Potential: ";
            PrintBranch(branch);
            if (CommitCompatible(branch, new_seqs[j])) {
              std::vector<proto::Transaction> seq(new_seqs[j]);
              seq.push_back(branch.txn());
              new_seqs1.push_back(seq);
            }
          }
        }
      }
      new_seqs.insert(new_seqs.end(), new_seqs1.begin(), new_seqs1.end());
    }
    for (const proto::Branch &branch : pending_branches) {
      if (branch.txn().id() == txns_sorted[txns_sorted.size() - 1]) {
        std::cerr << "Potential: ";
        PrintBranch(branch);
        for (const std::vector<proto::Transaction> &seq : new_seqs) {
          if (branch.seq().size() == 0 || CommitCompatible(branch, seq)) {
            proto::Branch new_branch(branch); 
            new_branch.clear_seq();
            for (const proto::Transaction &t : seq) {
              proto::Transaction *tseq = new_branch.add_seq();
              *tseq = t;
            }
            std::cerr << "Generated branch: ";
            PrintBranch(new_branch);
            if (std::find_if(already_generated.begin(), already_generated.end(),
                  [&](const proto::Branch &other) {
                    return google::protobuf::util::MessageDifferencer::Equals(
                        new_branch, other);
                  }) == already_generated.end()) {
              new_branches.push_back(new_branch);
            }
          }
        }
      }
    }
  } while (std::next_permutation(txns_sorted.begin(), txns_sorted.end()));
}

bool Server::WaitCompatible(const proto::Branch &branch, const std::vector<proto::Transaction> &seq) {
  std::vector<proto::Transaction> seq3;
  std::vector<proto::Transaction> seq4(seq);
  for (const proto::Transaction &b : branch.seq()) {
    auto itr = std::find_if(seq4.begin(), seq4.end(), [&](const proto::Transaction &other) {
          return google::protobuf::util::MessageDifferencer::Equals(b, other);
        });
    if (itr != seq4.end()) {
      seq4.erase(itr);
    }
    auto itr2 = std::find_if(seq.begin(), seq.end(), [&](const proto::Transaction &other) {
          return google::protobuf::util::MessageDifferencer::Equals(b, other);
        });
    if (itr2 != seq.end()) {
      seq3.push_back(b);
    }
  }
  return ValidSubsequence(branch.txn(), seq3, seq) &&
      NoConflicts(branch.txn(), seq4);
}

bool Server::CommitCompatible(const proto::Branch &branch,
    const std::vector<proto::Transaction> &seq) {
  std::vector<proto::Transaction> seq3;
  std::vector<proto::Transaction> seq4(seq);
  for (const proto::Transaction &b : branch.seq()) {
    seq3.push_back(b);
    auto itr = std::find_if(seq4.begin(), seq4.end(), [&](const proto::Transaction &other) {
          return google::protobuf::util::MessageDifferencer::Equals(b, other);
        });

    if (itr != seq4.end()) {
      seq4.erase(itr);
    }
  }
  return ValidSubsequence(branch.txn(), seq3, seq) &&
      NoConflicts(branch.txn(), seq4);
}

bool Server::ValidSubsequence(const proto::Transaction &txn,
      const std::vector<proto::Transaction> &seq1,
      const std::vector<proto::Transaction> &seq2) {
  int64_t k = -1;
  for (size_t i = 0; i < seq1.size(); ++i) {
    bool found = false;
    for (size_t j =  k + 1; j < seq2.size(); ++j) {
      if (seq1[i].id() == seq2[j].id()) {
        proto::Transaction txn1(seq2[j]);
        google::protobuf::RepeatedPtrField<proto::Operation> ops(txn1.ops());
        for (int l = 0; l < seq1[i].ops().size(); ++l) {
          const proto::Operation &op2 = seq1[i].ops()[l];
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
      const std::vector<proto::Transaction> &seq) {
  for (size_t i = 0; i < seq.size(); ++i) {
    if (TransactionsConflict(txn, seq[i])) {
      return false;
    }
  }
  return true;
}

bool Server::TransactionsConflict(const proto::Transaction &txn1,
      const proto::Transaction &txn2) {
  std::set<std::string> rs1;
  std::set<std::string> ws1;
  for (const proto::Operation &op : txn1.ops()) {
    if (op.type() == proto::OperationType::READ) {
      rs1.insert(op.key());
    } else {
      ws1.insert(op.key());
    }
  }
  std::set<std::string> rs2;
  std::set<std::string> ws2;
  for (const proto::Operation &op : txn2.ops()) {
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

void Server::ValueOnBranch(const proto::Branch &branch, const std::string &key,
    std::string &val) {
  if (ValueInTransaction(branch.txn(), key, val)) {
    return;
  } else if (branch.seq().size() > 0) {
    for (auto itr = branch.seq().rbegin(); itr != branch.seq().rend(); ++itr) {
      if (ValueInTransaction(*itr, key, val)) {
        return;
      }
    }
  }
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
