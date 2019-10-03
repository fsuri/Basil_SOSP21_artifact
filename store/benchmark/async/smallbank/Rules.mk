d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), smallbank_transaction.cc utils.cc smallbank_client.cc smallbank_generator.cc main.cc)

PROTOS += $(addprefix $(d), smallbank-proto.proto)

LIB-smallbank := $(o)smallbank_transaction.o $(o)utils.o $(o)smallbank-proto.o $(o)smallbank_client.o

$(d)smallbank_client: $(o)smallbank_transaction.o $(o)utils.o $(o)smallbank-proto.o $(o)smallbank_client.o $(o)main.o
$(d)smallbank_generator: $(LIB-io-utils) $(o)smallbank-proto.o $(o)smallbank_generator.o $(o)utils.o

BINS += $(addprefix $(d), smallbank_generator smallbank_client)

