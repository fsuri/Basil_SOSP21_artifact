d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), tpcc_client.cc tpcc_transaction.cc new_order.cc payment.cc order_status.cc stock_level.cc delivery.cc)

OBJ-sync-tpcc-transaction := $(LIB-store-frontend) $(o)tpcc_transaction.o

OBJ-sync-tpcc-client := $(o)tpcc_client.o

LIB-sync-tpcc := $(OBJ-sync-tpcc-client) $(OBJ-sync-tpcc-transaction) $(o)new_order.o \
	$(o)payment.o $(o)order_status.o $(o)stock_level.o $(o)delivery.o
