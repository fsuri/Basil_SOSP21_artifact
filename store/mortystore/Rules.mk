d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), client.cc server.cc store.cc shardclient.cc common.cc branch_generator.cc specstore.cc)

PROTOS += $(addprefix $(d), morty-proto.proto)

LIB-morty-store := $(o)server.o $(o)store.o \
	$(o)morty-proto.o $(o)common.o $(LIB-transport) $(LIB-store-backend) \
	$(LIB-store-common) $(o)branch_generator.o $(LIB-latency) $(o)specstore.o

LIB-morty-client := $(OBJS-ir-client) $(LIB-udptransport) $(LIB-store-frontend) \
	$(LIB-store-common) $(o)morty-proto.o $(o)client.o $(o)shardclient.o $(o)common.o \
	$(o)specstore.o

include $(d)tests/Rules.mk
