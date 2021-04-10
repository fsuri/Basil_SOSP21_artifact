d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), rw_client.cc rw_transaction.cc)

OBJ-rw-augustus-transaction := $(LIB-store-frontend) $(o)rw_transaction.o

OBJ-rw-augustus-client := $(o)rw_client.o

LIB-rw-augustus := $(OBJ-rw-augustus-client) $(OBJ-rw-augustus-transaction) 
