d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), client.cc shardclient.cc server.cc store.cc common.cc)

PROTOS += $(addprefix $(d), indicus-proto.proto)

LIB-indicus-store := $(o)server.o \
	$(o)indicus-proto.o  $(o)common.o $(LIB-crypto) $(LIB-bft-tapir-config) \
	$(LIB-configuration) $(LIB-store-common) $(LIB-transport)

LIB-indicus-client := $(LIB-udptransport) \
	$(LIB-store-frontend) $(LIB-store-common) $(o)indicus-proto.o \
	$(o)shardclient.o $(o)client.o $(LIB-bft-tapir-config) \
	$(LIB-crypto) $(o)common.o

include $(d)tests/Rules.mk
