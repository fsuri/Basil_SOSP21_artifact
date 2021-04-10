d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), benchmark.cc benchmark_oneshot.cc bench_client.cc async_transaction_bench_client.cc	sync_transaction_bench_client.cc)

OBJS-all-store-clients := $(OBJS-strong-client) $(OBJS-weak-client) \
		$(LIB-tapir-client) $(LIB-morty-client) $(LIB-janus-client) \
		$(LIB-indicus-client) $(LIB-pbft-store) $(LIB-hotstuff-store) $(LIB-augustus-store)

LIB-bench-client := $(o)benchmark.o $(o)bench_client.o \
		$(o)async_transaction_bench_client.o $(o)sync_transaction_bench_client.o

OBJS-all-bench-clients := $(LIB-retwis) $(LIB-tpcc) $(LIB-sync-tpcc) $(LIB-async-tpcc) \
	$(LIB-smallbank) $(LIB-rw)  

$(d)benchmark: $(LIB-key-selector) $(LIB-bench-client) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(OBJS-all-store-clients) $(OBJS-all-bench-clients) $(LIB-bench-client) $(LIB-store-common)

BINS +=  $(d)benchmark
