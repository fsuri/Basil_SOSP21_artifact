d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), bufferclient.cc asynctransaction.cc)

LIB-store-frontend := $(o)bufferclient.o $(o)asynctransaction.o
