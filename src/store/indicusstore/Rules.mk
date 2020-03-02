d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), client.cc shardclient.cc server.cc store.cc)

PROTOS += $(addprefix $(d), indicus-proto.proto)

LIB-indicus-store := $(OBJS-ir-replica) $(o)server.o $(o)store.o \
	$(o)indicus-proto.o 

LIB-indicus-client := $(OBJS-ir-client)  $(LIB-udptransport) \
	$(LIB-store-frontend) $(LIB-store-common) $(o)indicus-proto.o \
	$(o)shardclient.o $(o)client.o

