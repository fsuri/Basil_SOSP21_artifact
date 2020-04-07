#ifndef _PBFT_APP_H_
#define _PBFT_APP_H_

#include <string>
#include "store/pbftstore/pbft-proto.pb.h"
#include <google/protobuf/message.h>

namespace pbftstore {

class App {
public:

    App();
    virtual ~App();

    virtual ::google::protobuf::Message* HandleMessage(const std::string& type, const std::string& msg);
    // upcall to execute the message
    virtual ::google::protobuf::Message* Execute(const std::string& type, const std::string& msg, proto::CommitProof &&commitProof);
};

}

#endif /* _PBFT_APP_H_ */
