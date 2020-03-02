d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), ping_server.cc ping_client.cc)

PROTOS += $(addprefix $(d), ping-proto.proto)

$(d)ping_client: $(o)ping_client.o $(LIB-tcptransport) $(o)ping-proto.o $(LIB-latency)

$(d)ping_server: $(o)ping_server.o $(LIB-tcptransport) $(o)ping-proto.o $(LIB-latency)

BINS += $(d)ping_server $(d)ping_client 

