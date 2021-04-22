#include "store/bftsmartstore/bftsmartagent.h"

namespace bftsmartstore{
// client initialization
BftSmartAgent::BftSmartAgent(bool is_client, TransportReceiver* receiver, int id){
    // create Java VM
    create_java_vm();
    if (is_client){
        // create bft interface client
        create_interface_client(receiver, id);
    }
    else {
        // create bft interface client
        create_interface_server(receiver, id);
        Debug("finished creating an interface server...");
        // register natives
        register_natives();
    }
}

bool BftSmartAgent::create_java_vm(){
    using namespace std;
    JavaVM *this_jvm;
    JNIEnv *this_env;
    JavaVMInitArgs vm_args;
    JavaVMOption* options = new JavaVMOption[3];
<<<<<<< HEAD
    options[0].optionString = "-Djava.class.path=store/bftsmartstore/library/bin/BFT-SMaRt.jar:store/bftsmartstore/library/lib/slf4j-api-1.7.25.jar:store/bftsmartstore/library/lib/bcpkix-jdk15on-160.jar:store/bftsmartstore/library/lib/commons-codec-1.11.jar:store/bftsmartstore/library/lib/logback-classic-1.2.3.jar:store/bftsmartstore/library/lib/netty-all-4.1.34.Final.jar:store/bftsmartstore/library/lib/bcprov-jdk15on-160.jar:store/bftsmartstore/library/lib/core-0.1.4.jar:store/bftsmartstore/library/lib/logback-core-1.2.3.jar:store/bftsmartstore/library/config";
=======
    options[0].optionString = "-Djava.class.path=store/bftsmartstore/library/bin/BFT-SMaRt.jar:store/bftsmartstore/library/lib/slf4j-api-1.7.25.jar:store/bftsmartstore/library/lib/bcpkix-jdk15on-160.jar:store/bftsmartstore/library/lib/commons-codec-1.11.jar:store/bftsmartstore/library/lib/logback-classic-1.2.3.jar:store/bftsmartstore/library/lib/netty-all-4.1.34.Final.jar:store/bftsmartstore/library/lib/bcprov-jdk15on-160.jar:store/bftsmartstore/library/lib/core-0.1.4.jar:store/bftsmartstore/library/lib/logback-core-1.2.3.jar:store/bftsmartstore/library/config/";
>>>>>>> 5f5d5125b7cbd921e3a6b68864d7be1f7a12006a
    options[1].optionString = "-Dlogback.configurationFile=\"store/bftsmartstore/library/config/logback.xml\"";
    options[2].optionString = "-Djava.security.properties=\"store/bftsmartstore/library/config/java.security\"";
    // options[3].optionString = "-Dio.netty.tryReflectionSetAccessible=true";

    vm_args.version = JNI_VERSION_1_6;             // minimum Java version
    vm_args.nOptions = 3;                          // number of options
    vm_args.options = options;
    vm_args.ignoreUnrecognized = false;     // invalid options make the JVM init fail

    //=============== load and initialize Java VM and JNI interface =============
    jint rc = JNI_CreateJavaVM(&this_jvm, (void**)&this_env, &vm_args);  // YES !!
    this->jvm = this_jvm;
    this->env = this_env;
    delete options;    // we then no longer need the initialisation options.
    if (rc != JNI_OK) {
        // TO DO: error processing...
        // cin.get();
        // exit(EXIT_FAILURE);
        return false;
    }

    //=============== Display JVM version =======================================
    Debug("JVM load succeeded: Version ");
    jint ver = env->GetVersion();
    Debug("%d.%d", ((ver>>16)&0x0f), (ver&0x0f));
    return true;
}

bool BftSmartAgent::create_interface_client(TransportReceiver* receiver, int client_id){
    jclass cls = env->FindClass("bftsmart/demo/bftinterface/BftInterfaceClient");  // try to find the class
    if(cls == nullptr) {
        std::cerr << "ERROR: class not found !" << std::endl;
        return false;
    }
    else {                                  // if class found, continue
       Debug("Class BftInterfaceClient found. Client ID: %d", client_id);
       jmethodID mid = env->GetMethodID(cls, "<init>", "(IJ)V");  // find method
        if(mid == nullptr){
            std::cerr << "ERROR: constructor not found !" << std::endl;
            return false;
        }
        else {
            this->bft_client = env->NewObject(cls, mid, static_cast<jint>(client_id), reinterpret_cast<jlong>(receiver));                      // call method
        }
    }
    return true;
}

bool BftSmartAgent::create_interface_server(TransportReceiver* receiver, int server_id){
    jclass cls = env->FindClass("bftsmart/demo/bftinterface/BftInterfaceServer");  // try to find the class
    if(cls == nullptr) {
        std::cerr << "ERROR: class not found !" << std::endl;
        return false;
    }
    else {                                  // if class found, continue
       Debug("Class BftInterfaceServer found. Server ID: %d", server_id);

       jmethodID mid = env->GetMethodID(cls, "<init>", "(IJ)V");  // find method
        if(mid == nullptr){
            std::cerr << "ERROR: constructor not found !" << std::endl;
            return false;
        }
        else {
            Debug("method ID found!");
            this->bft_server = env->NewObject(cls, mid, static_cast<jint>(server_id), reinterpret_cast<jlong>(receiver)); // call method
            Debug("new bftsmart server object created! Yeeah!");
        }
    }
    return true;
}

void agent_request_received(JNIEnv* env, jobject arr){


    jclass server_cls = env->GetObjectClass(arr);
    jfieldID fid = env->GetFieldID(server_cls, "buffer", "Ljava/nio/ByteBuffer;");
    jobject buffer = env->GetObjectField(arr, fid);
    fid = env->GetFieldID(server_cls, "callbackHandle", "J");
    jlong handle = env->GetLongField(arr, fid);
    TransportReceiver* replica = reinterpret_cast<TransportReceiver*>(handle);
    ReplTransportAddress* repl_addr = new ReplTransportAddress("client", "");

    jlong capacity = env->GetDirectBufferCapacity(buffer);

    // jmethodID mid = env->GetMethodID(buffer_cls, "position", "()I");
    // Debug("getting mid!");

    // jint capacity = env->CallIntMethod(arr, mid);

    Debug("capacity: %d", capacity);

    char* req = static_cast<char*>(env->GetDirectBufferAddress(buffer));

    uint32_t *magic = reinterpret_cast<uint32_t*>(req);
    UW_ASSERT(*magic == MAGIC);

    size_t *sz = (size_t*) (req + sizeof(*magic));

    size_t totalSize = *sz;
    UW_ASSERT(totalSize < 1073741826);
    UW_ASSERT(totalSize == capacity);

    // Parse message
    char *ptr = req + sizeof(*sz) + sizeof(*magic);

    size_t typeLen = *((size_t *)ptr);
    ptr += sizeof(size_t);
    UW_ASSERT((size_t)(ptr-req) < totalSize);

    UW_ASSERT((size_t)(ptr+typeLen-req) < totalSize);
    string msgType(ptr, typeLen);
    ptr += typeLen;

    size_t msgLen = *((size_t *)ptr);
    ptr += sizeof(size_t);
    UW_ASSERT((size_t)(ptr-req) < totalSize);

    UW_ASSERT((size_t)(ptr+msgLen-req) <= totalSize);
    string msg(ptr, msgLen);
    ptr += msgLen;
    Debug("start sending the message to the receiver!");
    // replica->ReceiveFromBFTSmart(msgType, msg);
    replica->ReceiveMessage(*repl_addr, msgType, msg, nullptr);
}

bool BftSmartAgent::register_natives(){
    jclass cls = this->env->FindClass("bftsmart/demo/bftinterface/BftInterfaceServer");
    Debug("register natives started!");
    JNINativeMethod methods[] { { "bftRequestReceived", "(Lbftsmart/demo/bftinterface/BftInterfaceServer;)V", reinterpret_cast<void *>(agent_request_received) }};  // mapping table
    Debug("native registering!");

    if(this->env->RegisterNatives(cls, methods, sizeof(methods)/sizeof(JNINativeMethod)) < 0) {                        // register it
        if(this->env->ExceptionOccurred())                                        // verify if it's ok
        {
            Debug(" OOOOOPS: exception when registering natives");
            return false;
        }
        else
        {
            Debug(" ERROR: problem when registering natives");
            return false;
        }
    }
    Debug("succeeded in registering natives! ");
    return true;
}

void BftSmartAgent::send_to_group(ShardClient* recv, int group_idx, void * buffer, size_t size){
    // this->shard_client = recv;

    jbyteArray java_byte_array = this->env->NewByteArray(size);
    this->env->SetByteArrayRegion(java_byte_array, 0, size, reinterpret_cast<jbyte*>(buffer));

    jclass cls = this->env->GetObjectClass(this->bft_client);
    jmethodID mid = this->env->GetMethodID(cls, "startInterface", "([B)V");
    this->env->CallVoidMethod(this->bft_client, mid, java_byte_array);

}

void BftSmartAgent::destroy_java_vm(){
    this->jvm->DestroyJavaVM();
}

}
