d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), occstore.cc lockstore.cc server.cc \
					client.cc shardclient.cc)

PROTOS += $(addprefix $(d), strong-proto.proto)

OBJS-strong-client := $(OBJS-vr-client) $(LIB-udptransport) $(LIB-store-frontend) $(LIB-store-common) $(o)strong-proto.o $(o)shardclient.o $(o)client.o

LIB-strong-store := $(LIB-message) $(LIB-store-common) $(OBJS-vr-replica) \
	$(LIB-store-backend) $(o)strong-proto.o $(o)server.o $(o)occstore.o \
	$(o)lockstore.o

