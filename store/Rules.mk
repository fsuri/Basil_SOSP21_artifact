d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), server.cc)

$(d)server: $(LIB-tapir-store) $(LIB-strong-store) $(LIB-weak-store) \
	$(LIB-udptransport) $(LIB-tcptransport) $(OBJS-tapir-store) $(LIB-morty-store) $(o)server.o \
	$(OBJS-janus-store) $(LIB-io-utils) $(LIB-store-frontend) $(LIB-janus-client)

BINS += $(d)server
