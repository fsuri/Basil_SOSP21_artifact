d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), client.cc shardclient.cc server.cc store.cc common.cc \
		phase1validator.cc)

PROTOS += $(addprefix $(d), indicus-proto.proto)

LIB-indicus-store := $(o)server.o \
	$(o)indicus-proto.o  $(o)common.o $(LIB-crypto) $(LIB-bft-tapir-config) \
	$(LIB-configuration) $(LIB-store-common) $(LIB-transport) $(o)phase1validator.o

LIB-indicus-client := $(LIB-udptransport) \
	$(LIB-store-frontend) $(LIB-store-common) $(o)indicus-proto.o \
	$(o)shardclient.o $(o)client.o $(LIB-bft-tapir-config) \
	$(LIB-crypto) $(o)common.o $(o)phase1validator.o

include $(d)tests/Rules.mk
