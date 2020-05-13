#ifndef IR_TRANSPORT_H
#define IR_TRANSPORT_H

#include "lib/transport.h"
#include "lib/transportcommon.h"
#include "replication/ir/client.h"

namespace tapirstore {

template<class A>
class IRTransport : public TransportCommon<A> {
 public:
  IRTransport(replication::ir::IRClient *client);
  virtual ~IRTransport();
  
/*  virtual bool SendMessageInternal(TransportReceiver *src, const A &dst,
      const Message &m);*/
 private:
  replication::ir::IRClient *client;
};

} // namespace tapirstore

#endif /* IR_TRANSPORT_H */
