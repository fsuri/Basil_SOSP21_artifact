d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), \
		replica.cc client.cc config.cc)

PROTOS += $(addprefix $(d), \
	    messages-proto.proto)

LIB-bft-tapir-config := $(o)config.o

OBJS-replica-client := $(LIB-udptransport) $(LIB-message) $(LIB-crypto) \
		$(LIB-configuration) $(o)messages-proto.o $(o)config.o

$(d)replica: $(OBJS-replica-client) $(o)replica.o

$(d)client: $(OBJS-replica-client) $(o)client.o

BINS += $(d)replica
BINS += $(d)client
