d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), retwisClient.cc bench_client.cc)

OBJS-all-clients := $(OBJS-strong-client) $(OBJS-weak-client) $(OBJS-tapir-client) $(OBJS-morty-client)

$(d)benchClient: $(OBJS-all-clients)

$(d)retwisClient: $(OBJS-all-clients) $(LIB-retwis) $(o)bench_client.o

$(d)terminalClient: $(OBJS-all-clients)

# TODO: need to add back other clients (benchClient, terminalClient)
BINS += 

