#include <functional> 

namespace hotstuffstore {
    class IndicusInterface {
        typedef std::function<void(const std::string&)> hotstuff_exec_callback;

    public:
        void propose(const std::string& hash, hotstuff_exec_callback execb);
    };
}
