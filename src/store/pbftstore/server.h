#ifndef _PBFT_SERVER_H_
#define _PBFT_SERVER_H_

#include "store/pbftstore/app.h"
#include "store/server.h"
#include "lib/keymanager.h"

namespace pbftstore {

class Server : public App, public ::Server {
public:
  Server(KeyManager *keyManager, int groupIdx, int myId, bool signMessages, bool validateReads);
  ~Server();

  ::google::protobuf::Message* Execute(const std::string& type, const std::string& msg, proto::CommitProof &&commitProof);
  ::google::protobuf::Message* HandleMessage(const std::string& type, const std::string& msg);

  void Load(const std::string &key, const std::string &value,
      const Timestamp timestamp);

  Stats &GetStats();

private:
  Stats stats;
  KeyManager* keyManager;
  int groupIdx;
  int myId;
  bool signMessages;
  bool validateReads;
};

}

#endif
