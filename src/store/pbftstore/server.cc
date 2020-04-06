#include "store/pbftstore/server.h"

namespace pbftstore {

Server::Server() {

}

Server::~Server() {}

void Server::Execute(const std::string& type, const std::string& msg) {

}

void Server::Load(const std::string &key, const std::string &value,
    const Timestamp timestamp) {

}

Stats &Server::GetStats() {
  return stats;
}

}
