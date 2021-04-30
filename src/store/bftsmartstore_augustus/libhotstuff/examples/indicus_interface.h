#include <string>
#include <functional>
using std::string;

namespace bftsmartstore_augustus {
    class IndicusInterface {
        typedef std::function<void(const std::string&, uint32_t seqnum)> hotstuff_exec_callback;
        //const std::string config_dir_base = "/home/yunhao/florian/BFT-DB/src/store/bftsmartstore_augustus/libhotstuff/conf-indicus/";

        // on CloudLab
        const std::string config_dir_base = "/users/fs435/BFT-DB/src/store/bftsmartstore_augustus/libhotstuff/conf-indicus";

        int shardId;
        int replicaId;
        int cpuId;

        void initialize(int argc, char** argv);

    public:
        IndicusInterface(int shardId, int replicaId, int cpuId);

        void propose(const std::string& hash, hotstuff_exec_callback execb);
    };
}
