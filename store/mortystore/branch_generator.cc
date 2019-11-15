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

void BranchGenerator::AddPendingWrite(const std::string &key,
    const proto::Branch &branch) {
  pending_branches[branch.txn().id()].insert(branch);
}

void BranchGenerator::AddPendingRead(const std::string &key,
    const proto::Branch &branch) {
  pending_branches[branch.txn().id()].insert(branch);
}

void BranchGenerator::ClearPending(uint64_t txn_id) {
  pending_branches.erase(txn_id);
  for (auto itr = already_generated.begin(); itr != already_generated.end(); ) {
    if (itr->txn().id() == txn_id) {
      itr = already_generated.erase(itr);
    } else{
      ++itr;
    }
  }
}

void BranchGenerator::GenerateBranches(const proto::Branch &init,
    proto::OperationType type, const std::string &key,
    const SpecStore &store,
    std::vector<proto::Branch> &new_branches) {
  Latency_Start(&generateLatency);

  std::vector<proto::Branch> generated_branches;
  std::vector<uint64_t> txns_list;
  for (const auto &kv : pending_branches) {
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
  GenerateBranchesSubsets(txns_list, store, new_branches, subset);
  
  Latency_End(&generateLatency);
}

void BranchGenerator::GenerateBranchesSubsets(
    const std::vector<uint64_t> &txns, const SpecStore &store,
    std::vector<proto::Branch> &new_branches, std::vector<uint64_t> subset,
    int64_t i) {
  if (subset.size() > 0) {
    GenerateBranchesPermutations(subset, store, new_branches);
  }

  for (size_t j = i + 1; j < txns.size(); ++j) {
    subset.push_back(txns[j]);

    GenerateBranchesSubsets(txns, store, new_branches, subset, j);

    subset.pop_back();
  }
}

void BranchGenerator::GenerateBranchesPermutations(
    const std::vector<uint64_t> &txns,
    const SpecStore &store,
    std::vector<proto::Branch> &new_branches) {
  std::vector<uint64_t> txns_sorted(txns);
  std::sort(txns_sorted.begin(), txns_sorted.end());
  const proto::Transaction *t1;
  proto::Branch prev;
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
    std::vector<const std::vector<proto::Transaction> *> new_seqs;
    new_seqs.push_back(new std::vector<proto::Transaction>());

    for (size_t i = 0; i < txns_sorted.size() - 1; ++i) {
      std::vector<const std::vector<proto::Transaction> *> new_seqs1;
      for (size_t j = 0; j < new_seqs.size(); ++j) {
        auto itr = pending_branches.find(txns_sorted[i]);
        if (itr != pending_branches.end()) {
          for (const proto::Branch &branch : itr->second) {
            if (branch.txn().ops().size() == 1 || WaitCompatible(branch, store,
                  *new_seqs[j])) {
              std::vector<proto::Transaction> *seq =
                  new std::vector<proto::Transaction>(*new_seqs[j]);
              seq->push_back(branch.txn());
              new_seqs1.push_back(seq);
            }
          }
        }
      }
      new_seqs.insert(new_seqs.end(), new_seqs1.begin(), new_seqs1.end());
    }
    auto itr = pending_branches.find(txns_sorted[txns_sorted.size() - 1]);
    if (itr != pending_branches.end()) {
      Debug("Pending branches for %lu: %lu",
          txns_sorted[txns_sorted.size() - 1], itr->second.size());
      for (const proto::Branch &branch : itr->second) {
        prev = branch;
        prev.mutable_txn()->mutable_ops()->RemoveLast();
        if (Message_DebugEnabled(__FILE__)) {
          std::stringstream ss;
          ss << "  Potential: ";
          PrintBranch(branch, ss);
          Debug("%s", ss.str().c_str());

          ss.str("");

          ss << "  Prev: ";
          PrintBranch(prev, ss);
          Debug("%s", ss.str().c_str());
        }
        for (const std::vector<proto::Transaction> *seq : new_seqs) {
          if (Message_DebugEnabled(__FILE__)) {
            std::stringstream ss;
            ss << "  Seq: ";
            PrintTransactionList(*seq, ss);
            Debug("%s", ss.str().c_str());
          }
          if (WaitCompatible(prev, store, *seq)) {
            Debug("  Compatible");
            new_branch = branch;
            new_branch.clear_deps();
            for (const proto::Operation &op : new_branch.txn().ops()) {
              if (MostRecentConflict(op, store, *seq, t1)) {
                (*new_branch.mutable_deps())[t1->id()] = *t1;
              }
            }
            if (Message_DebugEnabled(__FILE__)) {
              std::stringstream ss;
              ss << "    Generated branch: ";
              PrintBranch(new_branch, ss);
              Debug("%s", ss.str().c_str());
            }
            Debug("Already generated: %lu", already_generated.size());
            if (already_generated.find(new_branch) == already_generated.end()) {
              new_branches.push_back(new_branch);
              already_generated.insert(new_branch);
            }
          }
        }
      }
    }
    for (size_t i = 0; i < new_seqs.size(); ++i) {
      delete new_seqs[i];
    }
  } while (std::next_permutation(txns_sorted.begin(), txns_sorted.end()));
}

} // namespace mortystore
