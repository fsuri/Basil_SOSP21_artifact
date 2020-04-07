#include "store/pbftstore/app.h"
#include "lib/assert.h"

namespace pbftstore {

App::App() {

}

App::~App() {

}

::google::protobuf::Message* App::Execute(const std::string &msg, const std::string &type, proto::CommitProof &&commitProof) {
  Panic("Unimplemented");
}

::google::protobuf::Message* App::HandleMessage(const std::string& type, const std::string& msg) {
  Panic("Unimplemented");
}

}
