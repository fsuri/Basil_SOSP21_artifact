#include "store/tapirstore/irtransport.h"

namespace tapirstore {

template<class A>
IRTransport<A>::IRTransport(replication::ir::IRClient *client) : client(client) {
}

template<class A>
IRTransport<A>::~IRTransport() {
}

} // namespace tapirstore
