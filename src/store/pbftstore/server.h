#ifndef _PBFT_SERVER_H_
#define _PBFT_SERVER_H_

#include "store/pbftstore/app.h"
#include "store/server.h"

namespace pbftstore {

class Server : public App, public ::Server {
public:
  Server();
  ~Server();

  void Execute(const std::string& type, const std::string& msg);


  void Load(const std::string &key, const std::string &value,
      const Timestamp timestamp);

  Stats &GetStats();

private:
  Stats stats;
};

}

#endif
