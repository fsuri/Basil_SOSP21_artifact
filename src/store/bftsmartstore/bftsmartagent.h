#include <jni.h>
#include "lib/repltransport.h"

#include <iostream>

namespace bftsmartstore{
class ShardClient;

    class BftSmartAgent{
public:
        BftSmartAgent();
        void destroy_java_vm(); 
        void send_to_group(ShardClient* recv, int group_idx, void * buffer, size_t size);       

private:
        JavaVM *jvm;
        JNIEnv *env;
        jobject bft_client;
        ShardClient* shard_client;

        bool create_java_vm();
        bool create_interface_client();
        bool register_natives();
    };
}