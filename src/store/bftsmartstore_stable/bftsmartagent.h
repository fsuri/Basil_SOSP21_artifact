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
#ifndef _BFTSMART_STABLE_AGENT_H_
#define _BFTSMART_STABLE_AGENT_H_


#include <jni.h>
#include "lib/repltransport.h"
#include "lib/message.h"

#include <iostream>
#include <sstream>
#define MAGIC 0x06121983

namespace bftsmartstore_stable{
class ShardClient;
const std::string remote_home = "/users/fs435";

    class BftSmartAgent{
public:
        static bool create_java_vm();
        BftSmartAgent(bool is_client, TransportReceiver* receiver, int id, int group_idx);
        ~BftSmartAgent();
        static void destroy_java_vm();
        void send_to_group(ShardClient* recv, int group_idx, void * buffer, size_t size);

private:
        static JavaVM *jvm;
        static JNIEnv *env;
        jobject bft_client;
        jobject bft_server;
        bool is_client;

        bool create_interface_client(TransportReceiver* receiver, int client_id, std::string config_home);
        bool create_interface_server(TransportReceiver* receiver, int server_id);
        bool register_natives();
    };
}

#endif
