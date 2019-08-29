d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), server.cc)

$(d)server: $(LIB-tapir-store) $(LIB-strong-store) $(LIB-weak-store) \
	$(LIB-udptransport) $(LIB-tcptransport) $(LIB-morty-store) $(o)server.o \
	$(LIB-janus-store) $(LIB-io-utils)

BINS += $(d)server
