d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), tpcc_client.cc tpcc_transaction.cc new_order.cc tpcc_generator.cc tpcc_utils.cc payment.cc order_status.cc stock_level.cc delivery.cc)

PROTOS += $(addprefix $(d), tpcc-proto.proto)

OBJ-tpcc-transaction := $(LIB-store-frontend) $(o)tpcc_transaction.o

OBJ-tpcc-client := $(o)tpcc_client.o

LIB-tpcc := $(OBJ-tpcc-client) $(OBJ-tpcc-transaction) $(o)new_order.o \
	$(o)tpcc-proto.o $(o)tpcc_utils.o $(o)payment.o $(o)order_status.o \
	$(o)stock_level.o

$(d)tpcc_generator: $(LIB-io-utils) $(o)tpcc-proto.o $(o)tpcc_generator.o $(o)tpcc_utils.o

BINS += $(d)tpcc_generator
