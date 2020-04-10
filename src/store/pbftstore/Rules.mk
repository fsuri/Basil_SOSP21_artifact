d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), app.cc replica.cc slots.cc common.cc server.cc shardclient.cc testreplica.cc testclient.cc)

PROTOS += $(addprefix $(d), pbft-proto.proto server-proto.proto)

LIB-pbft-store := $(o)server.o $(o)common.o $(o)slots.o $(o)replica.o \
	$(o)pbft-proto.o $(o)server-proto.o $(o)app.o $(o)shardclient.o $(LIB-crypto) \
	$(LIB-configuration) $(LIB-store-common) $(LIB-transport) \
	$(LIB-store-backend)

# LIB-pbft-client := $(LIB-udptransport) \
# 	$(LIB-store-frontend) $(LIB-store-common) $(o)pbft-proto.o \
# 	$(o)client.o $(LIB-bft-tapir-config) \
# 	$(LIB-crypto)

$(d)testserver: $(LIB-pbft-store) $(LIB-udptransport) $(o)testreplica.o
$(d)testclient: $(LIB-pbft-store) $(LIB-udptransport) $(o)testclient.o

BINS += $(d)testserver
BINS += $(d)testclient
