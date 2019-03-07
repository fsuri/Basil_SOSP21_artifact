d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), tpcc_transaction.cc new_order.cc tpccClient.cc)

OBJ-tpcc-transaction := $(LIB-store-frontend) $(o)tpcc_transaction.o

OBJS-all-clients := $(OBJS-strong-client) $(OBJS-weak-client) \
		$(OBJS-tapir-client) $(OBJS-morty-client)

LIB-tpcc := $(OBJ-tpcc-transaction) $(o)new_order.o

$(d)tpccClient: $(LIB-latency) $(OBJS-all-clients) $(LIB-tpcc) $(o)tpccClient.o

BINS += $(d)tpccClient

