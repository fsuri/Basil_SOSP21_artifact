d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), bufferclient.cc async_adapter_client.cc transaction_utils.cc sync_client.cc)

LIB-store-frontend := $(LIB-store-common) $(o)bufferclient.o \
		$(o)async_adapter_client.o $(o)transaction_utils.o $(o)sync_client.o
