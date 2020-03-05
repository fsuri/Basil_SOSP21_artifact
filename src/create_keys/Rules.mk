d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), \
		create_key.cc)

$(d)create_key: $(LIB-crypto) $(o)create_key.o

BINS += $(d)create_key