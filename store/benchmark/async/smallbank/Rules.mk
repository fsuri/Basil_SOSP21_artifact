d := $(dir $(lastword $(MAKEFILE_LIST)))

# TODO add all srcs
SRCS += $(addprefix $(d), smallbank_transaction.cc utils.cc smallbank_client.cc)

PROTOS += $(addprefix $(d), smallbank-proto.proto)

LIB-smallbank := $(o)smallbank_transaction.o $(o)utils.o $(o)smallbank-proto.o $(o)smallbank_client.o