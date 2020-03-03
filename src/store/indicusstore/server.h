// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicusstore/server.h:
 *   A single transactional server replica.
 *
 * Copyright 2015 Irene Zhang <iyzhang@cs.washington.edu>
 *                Naveen Kr. Sharma <naveenks@cs.washington.edu>
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

#ifndef _INDICUS_SERVER_H_
#define _INDICUS_SERVER_H_

#include "replication/ir/replica.h"
#include "store/server.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/indicusstore/store.h"
#include "store/indicusstore/indicus-proto.pb.h"

namespace indicusstore {

class Server : public TransportReceiver, public ::Server {
 public:
  Server(const transport::Configuration &config, int groupIdx, int idx,
      Transport *transport);
  virtual ~Server();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data,
      void *meta_data) override;

  virtual void Load(const string &key, const string &value,
      const Timestamp timestamp) override;

  virtual inline Stats &GetStats() override { return stats; }

 private:
  void HandleRead(const TransportAddress &remote, const proto::Read &msg);
  void HandlePrepare(const TransportAddress &remote, const proto::Prepare &msg);
  void HandleCommit(const TransportAddress &remote, const proto::Commit &msg);
  void HandleAbort(const TransportAddress &remote, const proto::Abort &msg);

  const transport::Configuration &config;
  int groupIdx;
  int idx;
  Transport *transport;
  Stats stats;

};

} // namespace indicusstore

#endif /* _INDICUS_SERVER_H_ */