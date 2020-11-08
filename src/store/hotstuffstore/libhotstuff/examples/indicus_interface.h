#include <functional> 

namespace hotstuffstore {
    class IndicusInterface {
        typedef std::function<void(const std::string&)> hotstuff_exec_callback;

        int shardId;
        int replicaId;

        void initialize(int argc, char** argv);
        
    public:
        IndicusInterface(int shardId, int replicaId);
        
        void propose(const std::string& hash, hotstuff_exec_callback execb);
    };
}
