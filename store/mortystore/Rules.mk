d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), client.cc server.cc store.cc shardclient.cc common.cc perm_branch_generator.cc specstore.cc branch_generator.cc lw_branch_generator.cc)

PROTOS += $(addprefix $(d), morty-proto.proto)

LIB-morty-store := $(o)server.o $(o)store.o $(o)branch_generator.o \
	$(o)morty-proto.o $(o)common.o $(LIB-transport) $(LIB-store-backend) \
	$(LIB-store-common) $(o)perm_branch_generator.o $(LIB-latency) $(o)specstore.o \
	$(o)lw_branch_generator.o

LIB-morty-client := $(OBJS-ir-client) $(LIB-udptransport) $(LIB-store-frontend) \
	$(LIB-store-common) $(o)morty-proto.o $(o)client.o $(o)shardclient.o $(o)common.o \
	$(o)specstore.o

include $(d)tests/Rules.mk
