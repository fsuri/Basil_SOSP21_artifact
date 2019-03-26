d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), bufferclient.cc async_adapter_client.cc)

LIB-store-frontend := $(LIB-store-common) $(o)bufferclient.o \
		$(o)async_adapter_client.o
