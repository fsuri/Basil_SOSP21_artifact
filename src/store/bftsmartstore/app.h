/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
 *                Zheng Wang <zw494@cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#ifndef _BFTSMART_APP_H_
#define _BFTSMART_APP_H_

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
