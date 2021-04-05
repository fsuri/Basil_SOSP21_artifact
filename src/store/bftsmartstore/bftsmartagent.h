#include <jni.h>
#include "lib/repltransport.h"

#include <iostream>
const uint32_t MAGIC = 0x06121983;


namespace bftsmartstore{
class ShardClient;

    class BftSmartAgent{
public:
        BftSmartAgent(bool is_client, TransportReceiver* receiver);
        void destroy_java_vm(); 
        void send_to_group(ShardClient* recv, int group_idx, void * buffer, size_t size);       

private:
        JavaVM *jvm;
        JNIEnv *env;
        jobject bft_client;
        jobject bft_server;

        bool create_java_vm();
        bool create_interface_client(TransportReceiver* receiver);
        bool create_interface_server(TransportReceiver* receiver);
        bool register_natives();
    };
}