d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), bufferclient.cc async_transaction.cc)

LIB-store-frontend := $(LIB-store-common) $(o)bufferclient.o \
		$(o)async_transaction.o
