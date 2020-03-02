// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/common/frontend/bufferclient.h:
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

#ifndef _BUFFER_CLIENT_H_
#define _BUFFER_CLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/promise.h"
#include "store/common/transaction.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/txnclient.h"

class BufferClient {
 public:
   BufferClient(TxnClient *txnclient);
   virtual ~BufferClient();

   // Begin a transaction with given tid.
   void Begin(uint64_t tid);

   // Get the value corresponding to key.
   virtual void Get(const std::string &key, get_callback gcb,
       get_timeout_callback gtcb, uint32_t timeout);

   // Set the value for the given key.
   virtual void Put(const std::string &key, const std::string &value,
       put_callback pcb, put_timeout_callback ptcb, uint32_t timeout);

   void Prepare(const Timestamp &timestamp, prepare_callback pcb,
       prepare_timeout_callback ptcb, uint32_t timeout);

   // Commit all Get(s) and Put(s) since Begin().
   virtual void Commit(uint64_t timestamp, commit_callback ccb,
       commit_timeout_callback ctcb, uint32_t timeout);

    
   // Abort all Get(s) and Put(s) since Begin().
   virtual void Abort(abort_callback acb, abort_timeout_callback atcb,
       uint32_t timeout);

 private:
  // Underlying single shard transaction client implementation.
  TxnClient* txnclient;

  // Transaction to keep track of read and write set.
  Transaction txn;
  
  // Unique transaction id to keep track of ongoing transaction.
  uint64_t tid;
};

#endif /* _BUFFER_CLIENT_H_ */
