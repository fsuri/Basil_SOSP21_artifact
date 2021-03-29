#include "store/bftsmartstore/bftsmartagent.h"

namespace bftsmartstore{

void BftSmartAgent::create_java_vm(){
    using namespace std;
    JavaVM *jvm;
    JNIEnv *env;
    JavaVMInitArgs vm_args;
    JavaVMOption* options = new JavaVMOption[1];
    options[0].optionString = "-Djava.class.path=.";

    vm_args.version = JNI_VERSION_1_6;             // minimum Java version
    vm_args.nOptions = 1;                          // number of options
    vm_args.options = options;
    vm_args.ignoreUnrecognized = false;     // invalid options make the JVM init fail
    
    //=============== load and initialize Java VM and JNI interface =============
    jint rc = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);  // YES !!
    delete options;    // we then no longer need the initialisation options. 
    if (rc != JNI_OK) {
        // TO DO: error processing... 
        cin.get();
        exit(EXIT_FAILURE);
    }
    
    //=============== Display JVM version =======================================
    cout << "JVM load succeeded: Version ";
    jint ver = env->GetVersion();
    cout << ((ver>>16)&0x0f) << "."<<(ver&0x0f) << endl;

}
}
