#include <string>
#include <functional> 

namespace hotstuffstore {
    class IndicusInterface {
        typedef std::function<void(const std::string&)> hotstuff_exec_callback;
        const std::string config_dir_base = "/home/yunhao/florian/BFT-DB/src/store/hotstuffstore/libhotstuff/conf-indicus/";

        int shardId;
        int replicaId;

        void initialize(int argc, char** argv);
        
    public:
        IndicusInterface(int shardId, int replicaId);
        
        void propose(const std::string& hash, hotstuff_exec_callback execb);
    };
}
