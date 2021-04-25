#include <jni.h>
#include "lib/repltransport.h"
#include "lib/message.h"

#include <iostream>
#include <sstream>
const uint32_t MAGIC = 0x06121983;


namespace bftsmartstore{
class ShardClient;

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