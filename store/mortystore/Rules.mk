d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), client.cc shardclient.cc replica_client.cc replica.cc store.cc)

PROTOS += $(addprefix $(d), morty-proto.proto)

LIB-morty-store := $(o)replica.o $(o)store.o \
	$(o)morty-proto.o

LIB-morty-client := $(OBJS-ir-client) $(LIB-udptransport) $(LIB-store-frontend) \
	$(LIB-store-common) $(o)morty-proto.o $(o)shardclient.o $(o)client.o \
	$(o)replica_client.o
