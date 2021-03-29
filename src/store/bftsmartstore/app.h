#ifndef _HOTSTUFF_APP_H_
#define _HOTSTUFF_APP_H_

#include <string>
#include "store/bftsmartstore/pbft-proto.pb.h"
#include <google/protobuf/message.h>
#include "store/common/stats.h"
#include <vector>

namespace bftsmartstore {

class App {
public:

    App();
    virtual ~App();

    virtual ::google::protobuf::Message* HandleMessage(const std::string& type, const std::string& msg);
    // upcall to execute the message
    virtual std::vector<::google::protobuf::Message*> Execute(const std::string& type, const std::string& msg);

    virtual Stats* mutableStats() = 0;
};

}

#endif /* _HOTSTUFF_APP_H_ */
