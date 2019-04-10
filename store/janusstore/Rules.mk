d := $(dir $(lastword $(MAKEFILE_LIST)))

# TODO replace with janus-specific stuff
SRCS += $(addprefix $(d), client.cc shardclient.cc transaction.cc server.cc store.cc)

PROTOS += $(addprefix $(d), janus-proto.proto)

OBJS-janus-store := $(LIB-message) $(LIB-store-common) $(LIB-store-backend) \
	$(o)janus-proto.o $(o)store.o $(o)transaction.o $(o)server.o

LIB-janus-store := $(OBJS-ir-replica) $(o)transaction.o $(o)server.o $(o)store.o \
	$(o)janus-proto.o

LIB-janus-client := $(OBJS-ir-client)  $(LIB-udptransport) \
	$(LIB-store-frontend) $(LIB-store-common) $(o)janus-proto.o \
	$(o)shardclient.o $(o)client.o

