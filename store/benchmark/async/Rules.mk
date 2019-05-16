d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), benchmark.cc benchmark_oneshot.cc bench_client.cc async_transaction_bench_client.cc)

OBJS-all-store-clients := $(OBJS-strong-client) $(OBJS-weak-client) \
		$(LIB-tapir-client) $(LIB-morty-client)

LIB-bench-client := $(o)benchmark.o $(o)bench_client.o \
		$(o)async_transaction_bench_client.o

OBJS-all-bench-clients := $(LIB-retwis) $(LIB-tpcc)

$(d)benchmark: $(LIB-key-selector) $(LIB-bench-client) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(OBJS-all-store-clients) $(OBJS-all-bench-clients) $(LIB-bench-client)

$(d)benchmark_oneshot: $(LIB-janus-client) $(o)benchmark_oneshot.o

BINS +=  $(d)benchmark $(d)benchmark_oneshot
