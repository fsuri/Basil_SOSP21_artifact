d := $(dir $(lastword $(MAKEFILE_LIST)))

GTEST_SRCS += $(addprefix $(d), smallbank_transaction_test.cc smallbank_generator_test.cc smallbank_client_test.cc ../../sync_transaction_bench_client.cc ../../bench_client.cc)

$(d)smallbank_generator_test: $(o)smallbank_generator_test.o \
	$(LIB-io-utils) $(o)../smallbank_generator.o $(o)../smallbank-proto.o $(o)../utils.o \
	$(GTEST_MAIN)
$(d)smallbank_client_test: $(LIB-io-utils) $(LIB-bench-client) $(LIB-store-frontend) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(LIB-smallbank) $(o)../../sync_transaction_bench_client.o $(o)../../bench_client.o $(o)smallbank_client_test.o $(GTEST_MAIN)
$(d)smallbank_transaction_test: $(LIB-io-utils) $(LIB-bench-client) $(LIB-store-frontend) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(LIB-smallbank) $(o)../../sync_transaction_bench_client.o $(o)../../bench_client.o $(o)smallbank_transaction_test.o $(GTEST_MAIN) $(GMOCK) $(GTEST)

TEST_BINS +=  $(addprefix $(d), smallbank_generator_test smallbank_client_test smallbank_transaction_test)
