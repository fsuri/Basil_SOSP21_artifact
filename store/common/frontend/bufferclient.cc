// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/common/frontend/bufferclient.cc:
 *   Single shard buffering client implementation.
 *
 * Copyright 2015 Irene Zhang <iyzhang@cs.washington.edu>
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

#include "store/common/frontend/bufferclient.h"

using namespace std;

BufferClient::BufferClient(TxnClient* txnclient) : txn()
{
    this->txnclient = txnclient;
}

BufferClient::~BufferClient() { }

/* Begins a transaction. */
void
BufferClient::Begin(uint64_t tid)
{
    // Initialize data structures.
    txn = Transaction();
    this->tid = tid;
    txnclient->Begin(tid);
}

void BufferClient::Get(const std::string &key, get_callback gcb,
    get_timeout_callback gtcb, uint32_t timeout) {

  // Read your own writes, check the write set first.
  if (txn.getWriteSet().find(key) != txn.getWriteSet().end()) {
    gcb(REPLY_OK, key, (txn.getWriteSet().find(key))->second, Timestamp());
    return;
  }

  // Consistent reads, check the read set.
  if (txn.getReadSet().find(key) != txn.getReadSet().end()) {
    // read from the server at same timestamp.
    txnclient->Get(tid, key, (txn.getReadSet().find(key))->second, gcb, gtcb,
        timeout);
    return;
  }
  
  get_callback bufferCb = [this, gcb](int status, const std::string & key,
      const std::string &value, Timestamp ts) {
    if (status == REPLY_OK) {
      this->txn.addReadSet(key, ts);
    }
    gcb(status, key, value, ts);
  };
  txnclient->Get(tid, key, bufferCb, gtcb, GET_TIMEOUT);
}

void BufferClient::Put(const std::string &key, const std::string &value,
    put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) {
  txn.addWriteSet(key, value);
  pcb(REPLY_OK, key, value);
}

void BufferClient::Prepare(const Timestamp &timestamp, prepare_callback pcb,
       prepare_timeout_callback ptcb, uint32_t timeout) {
  txnclient->Prepare(tid, txn, timestamp, pcb, ptcb, timeout);
}

void BufferClient::Commit(uint64_t timestamp, commit_callback ccb,
    commit_timeout_callback ctcb, uint32_t timeout) {
  txnclient->Commit(tid, txn, timestamp, ccb, ctcb, timeout);
}
    
void BufferClient::Abort(abort_callback acb, abort_timeout_callback atcb,
    uint32_t timeout) {
  txnclient->Abort(tid, Transaction(), acb, atcb, timeout);
}
