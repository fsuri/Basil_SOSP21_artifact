#include "store/pbftstore/app.h"
#include "lib/assert.h"

namespace pbftstore {

App::App() {

}

App::~App() {

}

std::vector<::google::protobuf::Message*> App::Execute(const std::string &msg, const std::string &type) {
  Panic("Unimplemented");
}

::google::protobuf::Message* App::HandleMessage(const std::string& type, const std::string& msg) {
  Panic("Unimplemented");
}

}
