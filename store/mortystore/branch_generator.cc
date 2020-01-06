#include "store/mortystore/branch_generator.h"

#include <sstream>

#include "lib/message.h"
#include "store/mortystore/common.h"

#include <google/protobuf/util/message_differencer.h>

namespace mortystore {

BranchGenerator::BranchGenerator() {
  _Latency_Init(&generateLatency, "branch_generation");
}

BranchGenerator::~BranchGenerator() {
  //Latency_Dump(&generateLatency);
}

void BranchGenerator::AddPending(const proto::Branch &branch) {
  for (size_t i = 0; i < branch.txn().ops_size(); ++i) {
    const proto::Operation &op = branch.txn().ops(i);
    if (op.type() == proto::OperationType::READ) {
      AddPendingRead(op.key(), branch);
    } else {
      AddPendingWrite(op.key(), branch);
    }
  }
}

void BranchGenerator::AddPendingWrite(const std::string &key,
    const proto::Branch &branch) {
  pending_writes[key][branch.txn().id()].insert(branch);
}

void BranchGenerator::AddPendingRead(const std::string &key,
    const proto::Branch &branch) {
  pending_reads[key][branch.txn().id()].insert(branch);
}

void BranchGenerator::ClearPending(uint64_t txn_id) {
  for (auto &kv : pending_writes) {
    kv.second.erase(txn_id);
  }
  for (auto &kv : pending_reads) {
    kv.second.erase(txn_id);
  }
  for (auto itr = already_generated.begin(); itr != already_generated.end(); ) {
    if (itr->txn().id() == txn_id) {
      itr = already_generated.erase(itr);
    } else{
      ++itr;
    }
  }
}

uint64_t BranchGenerator::GenerateBranches(const proto::Branch &init,
    proto::OperationType type, const std::string &key,
    const SpecStore &store,
    std::vector<proto::Branch> &new_branches) {
  Latency_Start(&generateLatency);

  std::vector<proto::Branch> generated_branches;

  BranchMap p_branches;
  p_branches[init.txn().id()].insert(init);
  for (const auto &op : init.txn().ops()) {
    for (const auto &kv : pending_writes[op.key()]) { 
      p_branches[kv.first].insert(kv.second.begin(), kv.second.end());
    }
    if (op.type() == proto::OperationType::WRITE) {
      for (const auto &kv : pending_reads[op.key()]) { 
        p_branches[kv.first].insert(kv.second.begin(), kv.second.end());
      }
    }
  }

  std::vector<uint64_t> txns_list;
  for (const auto &kv : p_branches) {
    if (kv.first != init.txn().id()) {
      // only generate subsets that include init
      txns_list.push_back(kv.first); 
    }
  }

  Debug("Concurrent transactions: %lu", txns_list.size());
  /*if (Message_DebugEnabled(__FILE__)) {
    std::stringstream ss;
    ss << "Committed: ";
    PrintTransactionList(committed, ss);
    Debug("%s", ss.str().c_str());
  }*/

  std::vector<uint64_t> subset = {init.txn().id()};
  GenerateBranchesSubsets(txns_list, p_branches, store, new_branches, subset);
  
  return Latency_End(&generateLatency);
}

void BranchGenerator::GenerateBranchesSubsets(
    const std::vector<uint64_t> &txns,
    const BranchMap &p_branches,
    const SpecStore &store,
    std::vector<proto::Branch> &new_branches, std::vector<uint64_t> subset,
    int64_t i) {
  if (subset.size() > 0) {
    GenerateBranchesPermutations(subset, p_branches, store, new_branches);
  }

  for (size_t j = i + 1; j < txns.size(); ++j) {
    subset.push_back(txns[j]);

    GenerateBranchesSubsets(txns, p_branches, store, new_branches, subset, j);

    subset.pop_back();
  }
}

void BranchGenerator::GenerateBranchesPermutations(
    const std::vector<uint64_t> &txns,
    const BranchMap &p_branches,
    const SpecStore &store,
    std::vector<proto::Branch> &new_branches) {
  std::vector<uint64_t> txns_sorted(txns);
  std::sort(txns_sorted.begin(), txns_sorted.end());
  const proto::Transaction *t1;
  proto::Branch new_branch; 
  do {
    if (Message_DebugEnabled(__FILE__)) {
      std::stringstream ss;
      ss << "Permutation: [";
      for (size_t i = 0; i < txns_sorted.size(); ++i) {
        ss << txns_sorted[i];
        if (i < txns_sorted.size() - 1) {
          ss << ", ";
        }
      }
      ss << "]";
      Debug("%s", ss.str().c_str());
    }
    std::unordered_set<std::vector<proto::Transaction>, TransactionVectorHasher, TransactionVectorComparer> prev_seqs;
    prev_seqs.insert(std::vector<proto::Transaction>());

    for (size_t i = 0; i < txns_sorted.size() - 1; ++i) {
      std::unordered_set<std::vector<proto::Transaction>, TransactionVectorHasher, TransactionVectorComparer> next_seqs;
      for (const std::vector<proto::Transaction> &prev_seqsj : prev_seqs) {
        auto itr = p_branches.find(txns_sorted[i]);
        if (itr != p_branches.end()) {
          for (const proto::Branch &branch : itr->second) {
            if (branch.txn().ops().size() == 1 || WaitCompatible(branch, store,
                  prev_seqsj)) {
              std::vector<proto::Transaction> seq(prev_seqsj);
              seq.push_back(branch.txn());
              next_seqs.insert(seq);
            }
          }
        }
      }
      prev_seqs.clear();
      prev_seqs.insert(next_seqs.begin(), next_seqs.end());
    }
    Debug("Generated seq prefixes: %lu", prev_seqs.size());
    auto itr = p_branches.find(txns_sorted[txns_sorted.size() - 1]);
    if (itr != p_branches.end()) {
      Debug("Pending branches for %lu: %lu",
          txns_sorted[txns_sorted.size() - 1], itr->second.size());
      for (const proto::Branch &branch : itr->second) {
        if (Message_DebugEnabled(__FILE__)) {
          std::stringstream ss;
          ss << "  Potential: ";
          PrintBranch(branch, ss);
          Debug("%s", ss.str().c_str());
        }
        for (const std::vector<proto::Transaction> &seq : prev_seqs) {
          if (Message_DebugEnabled(__FILE__)) {
            std::stringstream ss;
            ss << "  Seq: ";
            PrintTransactionList(seq, ss);
            Debug("%s", ss.str().c_str());
          }
          if (WaitCompatible(branch, store, seq, true)) {
            Debug("  Compatible");
            new_branch.set_id(branch.id());
            *new_branch.mutable_txn() = branch.txn();
            *new_branch.mutable_shards() = branch.shards();
            new_branch.clear_deps();
            for (const proto::Operation &op : new_branch.txn().ops()) {
              if (MostRecentConflict(op, store, seq, t1)) {
                (*new_branch.mutable_deps())[t1->id()] = *t1;
              }
            }
            if (Message_DebugEnabled(__FILE__)) {
              std::stringstream ss;
              ss << "    Generated branch: ";
              PrintBranch(new_branch, ss);
              Debug("%s", ss.str().c_str());

              /*ss.str("");
              Debug("Already generated: %lu", already_generated.size());
              for (const auto &ag : already_generated) {
                ss << std::endl;
                PrintBranch(ag, ss);
              }
              Debug("%s", ss.str().c_str());*/
            }
            if (already_generated.find(new_branch) == already_generated.end()) {
              Debug("    Not previously generated.");
              new_branches.push_back(new_branch);
              already_generated.insert(new_branch);
            }
          }
        }
      }
    }
  } while (std::next_permutation(txns_sorted.begin(), txns_sorted.end()));
}

} // namespace mortystore
