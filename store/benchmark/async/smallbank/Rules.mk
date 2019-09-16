d := $(dir $(lastword $(MAKEFILE_LIST)))

# TODO add all srcs
SRCS += $(addprefix $(d), client.cc utils.cc)

PROTOS += $(addprefix $(d), database.proto)

LIB-smallbank := $(o)client.o $(o)utils.o