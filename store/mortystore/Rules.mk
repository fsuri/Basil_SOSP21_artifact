d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), specclient.cc shardclient.cc)

PROTOS += $(addprefix $(d), morty-proto.proto)

OBJS-morty-store := $(LIB-message) $(LIB-store-common) $(LIB-store-backend) \
	  $(o)morty-proto.o

OBJS-morty-client := $(OBJS-ir-client)  $(LIB-udptransport) $(LIB-store-frontend) $(LIB-store-common) \
		$(o)morty-proto.o $(o)shardclient.o $(o)specclient.o

$(d)server: $(LIB-udptransport) $(OBJS-ir-replica) \
		$(OBJS-morty-store)

BINS +=
