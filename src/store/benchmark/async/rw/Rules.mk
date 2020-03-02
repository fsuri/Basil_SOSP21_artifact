d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), rw_client.cc rw_transaction.cc)

OBJ-rw-transaction := $(LIB-store-frontend) $(o)rw_transaction.o

OBJ-rw-client := $(o)rw_client.o

LIB-rw := $(OBJ-rw-client) $(OBJ-rw-transaction) 
