d := $(dir $(lastword $(MAKEFILE_LIST)))

# TODO add all srcs
SRCS += $(addprefix $(d), client.cc utils.cc)

PROTOS += $(addprefix $(d), smallbank-proto.proto)

LIB-smallbank := $(o)client.o $(o)utils.o $(o)smallbank-proto.o