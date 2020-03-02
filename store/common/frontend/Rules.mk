d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), bufferclient.cc async_adapter_client.cc transaction_utils.cc sync_client.cc async_one_shot_adapter_client.cc one_shot_transaction.cc)

LIB-store-frontend := $(LIB-store-common) $(o)bufferclient.o \
		$(o)async_adapter_client.o $(o)transaction_utils.o $(o)sync_client.o \
		$(o)async_one_shot_adapter_client.o $(o)one_shot_transaction.o
