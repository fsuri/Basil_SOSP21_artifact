d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), app.cc replica.cc slots.cc common.cc server.cc shardclient.cc client.cc testreplica.cc testclient.cc pbft_batched_sigs.cc bftsmartagent.cc)

PROTOS += $(addprefix $(d), pbft-proto.proto server-proto.proto)

# HotStuff static libraries
LIB-hotstuff-interface := store/bftsmartstore/libhotstuff/examples/libindicus_interface.a store/bftsmartstore/libhotstuff/salticidae/libsalticidae.a store/bftsmartstore/libhotstuff/libhotstuff.a store/bftsmartstore/libhotstuff/secp256k1/.libs/libsecp256k1.a


LIB-pbft-batched-sigs := $(LIB-crypto) $(o)pbft_batched_sigs.o 

LIB-bftsmart-store := $(o)common.o $(o)slots.o $(o)replica.o $(o)server.o \
	$(o)pbft-proto.o $(o)server-proto.o $(o)app.o $(o)bftsmartagent.o $(o)shardclient.o \
	$(o)client.o $(LIB-crypto) $(LIB-pbft-batched-sigs) $(LIB-configuration) $(LIB-store-common) \
	$(LIB-transport) $(LIB-store-backend) $(LIB-hotstuff-interface)

# LIB-pbft-client := $(LIB-udptransport) \
# 	$(LIB-store-frontend) $(LIB-store-common) $(o)pbft-proto.o \
# 	$(o)client.o $(LIB-bft-tapir-config) \
# 	$(LIB-crypto)

#$(d)testserver: $(LIB-pbft-store) $(LIB-udptransport) $(o)testreplica.o
#$(d)testclient: $(LIB-pbft-store) $(LIB-udptransport) $(o)testclient.o

#BINS += $(d)testserver
#BINS += $(d)testclient
