// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicusstore/server.cc:
 *   Implementation of a single transactional key-value server.
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

#include "store/indicusstore/server.h"

#include "lib/tcptransport.h"

namespace indicusstore {

Server::Server(const transport::Configuration &config, int groupIdx, int idx,
    Transport *transport) : config(config),
    groupIdx(groupIdx), idx(idx), transport(transport) {
  transport->Register(this, config, groupIdx, idx);
}

Server::~Server() {
}

void Server::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) {
  proto::Read read;
  proto::Prepare prepare;
  proto::Commit commit;
  proto::Abort abort;

  if (type == read.GetTypeName()) {
    read.ParseFromString(data);
    HandleRead(remote, read);
  } else if (type == prepare.GetTypeName()) {
    prepare.ParseFromString(data);
    HandlePrepare(remote, prepare);
  } else if (type == commit.GetTypeName()) {
    commit.ParseFromString(data);
    HandleCommit(remote, commit);
  } else if (type == abort.GetTypeName()) {
    abort.ParseFromString(data);
    HandleAbort(remote, abort);
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}


void Server::Load(const string &key, const string &value,
    const Timestamp timestamp) {
}

void Server::HandleRead(const TransportAddress &remote, const proto::Read &msg) {
}

void Server::HandlePrepare(const TransportAddress &remote,
    const proto::Prepare &msg) {
}

void Server::HandleCommit(const TransportAddress &remote,
    const proto::Commit &msg) {
}

void Server::HandleAbort(const TransportAddress &remote,
    const proto::Abort &msg) {
}

} // namespace indicusstore
