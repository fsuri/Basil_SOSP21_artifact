#ifndef MORTY_SERVER_H
#define MORTY_SERVER_H

#include "store/server.h"
#include "store/common/backend/txnstore.h"

namespace mortystore {

class Server : public ::Server {
 public:
  Server();
  virtual ~Server();

  virtual void Load(const std::string &key, const std::string &value,
      const Timestamp timestamp) override;

 private:
  TxnStore *store;
};

} // namespace mortystore

#endif /* MORTY_SERVER_H */
