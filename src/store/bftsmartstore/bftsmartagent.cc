#include "store/bftsmartstore/bftsmartagent.h"

namespace bftsmartstore{
// client initialization
BftSmartAgent::BftSmartAgent(bool is_client, TransportReceiver* receiver){
    // create Java VM
    create_java_vm();
    if (is_client){
        // create bft interface client
        create_interface_client(receiver);
    }
    else {
        // create bft interface client
        create_interface_server(receiver);
        // register natives
        register_natives();
    }
}

bool BftSmartAgent::create_java_vm(){
    using namespace std;
    JavaVM *this_jvm;
    JNIEnv *this_env;
    JavaVMInitArgs vm_args;
    JavaVMOption* options = new JavaVMOption[1];
    options[0].optionString = "-Djava.class.path=library/bin/bftsmart/demo/bftinterface";

    vm_args.version = JNI_VERSION_1_6;             // minimum Java version
    vm_args.nOptions = 1;                          // number of options
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
    std::cout << "JVM load succeeded: Version ";
    jint ver = env->GetVersion();
    std::cout << ((ver>>16)&0x0f) << "."<<(ver&0x0f) << std::endl;
    return true;
}

bool BftSmartAgent::create_interface_client(TransportReceiver* receiver){
    jclass cls = env->FindClass("BftInterfaceClient");  // try to find the class
    if(cls == nullptr) {
        std::cerr << "ERROR: class not found !";
        return false;
    }
    else {                                  // if class found, continue
       std::cout << "Class BftInterfaceClient found" << std::endl;
       jmethodID mid = env->GetMethodID(cls, "<init>", "(IJ)V");  // find method
        if(mid == nullptr){
            std::cerr << "ERROR: constructor not found !" << std::endl;
            return false;
        }
        else {
            this->bft_client = env->NewObject(cls, mid, static_cast<jint>(1001), reinterpret_cast<jlong>(receiver));                      // call method
            std::cout << std::endl;
        }
    }
    return true;
}

bool BftSmartAgent::create_interface_server(TransportReceiver* receiver){
    jclass cls = env->FindClass("BftInterfaceServer");  // try to find the class
    if(cls == nullptr) {
        std::cerr << "ERROR: class not found !";
        return false;
    }
    else {                                  // if class found, continue
       std::cout << "Class BftInterfaceServer found" << std::endl;
       jmethodID mid = env->GetMethodID(cls, "<init>", "(IJ)V");  // find method
        if(mid == nullptr){
            std::cerr << "ERROR: constructor not found !" << std::endl;
            return false;
        }
        else {
            this->bft_server = env->NewObject(cls, mid, static_cast<jint>(1001), reinterpret_cast<jlong>(receiver));                      // call method
            std::cout << std::endl;
        }
    }
    return true;
}

void agent_request_received(JNIEnv* env, jbyteArray arr, jlong handle){
    jbyte* buf = env->GetByteArrayElements(arr, NULL);
    TransportReceiver* replica = reinterpret_cast<TransportReceiver*>(handle);
    ReplTransportAddress* repl_addr = new ReplTransportAddress("client", "");
    char * req = reinterpret_cast<char*>(buf);
    uint32_t *magic = reinterpret_cast<uint32_t*>(req);
    UW_ASSERT(*magic == MAGIC);

    size_t *sz = (size_t*) (req + sizeof(*magic));

    size_t totalSize = *sz;
    UW_ASSERT(totalSize < 1073741826);

    if (strlen(req) < totalSize) {
        return;
    }

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
    replica->ReceiveMessage(*repl_addr, msgType, msg, nullptr);
}

bool BftSmartAgent::register_natives(){
    jclass cls = this->env->FindClass("BftInterfaceServer");
    JNINativeMethod methods[] { { "bftRequestReceived", "([BJ)V", (void *)&agent_request_received } };  // mapping table

    if(this->env->RegisterNatives(cls, methods, 1) < 0) {                        // register it
    if(this->env->ExceptionOccurred())                                        // verify if it's ok
    {
        std::cerr << " OOOOOPS: exception when registreing naives" << std::endl;
        return false;
    }
    else
    {
        std::cerr << " ERROR: problem when registreing naives" << std::endl;
        return false;
    }
    return true;
}
}

void BftSmartAgent::send_to_group(ShardClient* recv, int group_idx, void * buffer, size_t size){
    // this->shard_client = recv;
    
    jbyteArray java_byte_array = this->env->NewByteArray(size);
    this->env->SetByteArrayRegion(java_byte_array, 0, size, reinterpret_cast<jbyte*>(buffer));

    jclass cls = this->env->FindClass("BftInterfaceClient");
    jmethodID mid = this->env->GetMethodID(cls, "startInterface", "([B)V");
    this->env->CallVoidMethod(this->bft_client, mid, java_byte_array);

}

void BftSmartAgent::destroy_java_vm(){
    this->jvm->DestroyJavaVM();
}

}
