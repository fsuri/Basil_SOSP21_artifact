// note, rewriting common/transaction.h because unneeded timestamp
// #include "store/common/transaction.h"

#ifndef _JANUS_TRANSACTION_H_
#define _JANUS_TRANSACTION_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/common-proto.pb.h"
#include "store/janusstore/janus-proto.pb.h"

#include <unordered_map>

// Reply types
#define REPLY_OK 0
#define REPLY_FAIL 1
#define REPLY_RETRY 2
#define REPLY_ABSTAIN 3
#define REPLY_TIMEOUT 4
#define REPLY_NETWORK_FAILURE 5
#define REPLY_MAX 6

namespace janusstore {


/* Defines a transaction that is forwarded to a particular replica */
class Transaction {
private:
    // the unique transaction ID
    uint64_t txn_id;

    janusstore::proto::TransactionMessage::Status status;

public:
    // the server this txn is associated with
    std::string server_ip;
    uint64_t server_port;
    Transaction() {};
    Transaction(uint64_t txn_id, std::string server_ip, uint64_t server_port);
    Transaction(uint64_t txn_id, std::string server_ip, uint64_t server_port, const TransactionMessage &msg);
    ~Transaction();

    // set of keys to be read
    std::unordered_set<std::string> read_set;

    // map between key and value(s)
    std::unordered_map<std::string, std::string> write_set;

    void setTransactionId(uint64_t txn_id);
    void setTransactionStatus(janusstore::proto::TransactionMessage::Status status);
    const uint64_t getTransactionId() const;
    const janusstore::proto::TransactionMessage::Status getTransactionStatus() const;
    const std::unordered_set<std::string>& getReadSet() const;
    const std::unordered_map<std::string, std::string>& getWriteSet() const;

    inline size_t GetNumRead() const { return read_set.size(); }
    inline size_t GetNumWritten() const { return write_set.size(); }

    void addReadSet(const std::string &key);
    void addWriteSet(const std::string &key, const std::string &value);
    void serialize(janusstore::proto::TransactionMessage *msg) const;
};

} // namespace janusstore

#endif /* _JANUS_TRANSACTION_H_ */
