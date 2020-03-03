d := $(dir $(lastword $(MAKEFILE_LIST)))

GTEST_SRCS += $(addprefix $(d), amalgamate_test.cc bal_test.cc deposit_test.cc transact_test.cc write_check_test.cc utils_test.cc smallbank_transaction_test.cc smallbank_generator_test.cc smallbank_client_test.cc ../../sync_transaction_bench_client.cc ../../bench_client.cc)

$(d)smallbank_generator_test: $(o)smallbank_generator_test.o \
	$(LIB-io-utils) $(o)../smallbank_generator.o $(o)../smallbank-proto.o $(o)../utils.o \
	$(GTEST_MAIN)
$(d)smallbank_client_test: $(LIB-io-utils) $(LIB-bench-client) $(LIB-store-frontend) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(LIB-smallbank) $(o)../../sync_transaction_bench_client.o $(o)../../bench_client.o $(o)smallbank_client_test.o $(GTEST_MAIN)
$(d)smallbank_utils_test: $(LIB-io-utils) $(LIB-bench-client) $(LIB-store-frontend) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(LIB-smallbank)  $(o)../../sync_transaction_bench_client.o $(o)../../bench_client.o $(o)utils_test.o $(GTEST_MAIN) $(GMOCK) $(GTEST)
$(d)amalgamate_test: $(LIB-io-utils) $(LIB-bench-client) $(LIB-store-frontend) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(LIB-smallbank)  $(o)../../sync_transaction_bench_client.o $(o)../../bench_client.o $(o)amalgamate_test.o $(GTEST_MAIN) $(GMOCK) $(GTEST)
$(d)bal_test: $(LIB-io-utils) $(LIB-bench-client) $(LIB-store-frontend) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(LIB-smallbank)  $(o)../../sync_transaction_bench_client.o $(o)../../bench_client.o $(o)bal_test.o $(GTEST_MAIN) $(GMOCK) $(GTEST)
$(d)deposit_test: $(LIB-io-utils) $(LIB-bench-client) $(LIB-store-frontend) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(LIB-smallbank)  $(o)../../sync_transaction_bench_client.o $(o)../../bench_client.o $(o)deposit_test.o $(GTEST_MAIN) $(GMOCK) $(GTEST)
$(d)transact_test: $(LIB-io-utils) $(LIB-bench-client) $(LIB-store-frontend) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(LIB-smallbank)  $(o)../../sync_transaction_bench_client.o $(o)../../bench_client.o $(o)transact_test.o $(GTEST_MAIN) $(GMOCK) $(GTEST)
$(d)write_check_test: $(LIB-io-utils) $(LIB-bench-client) $(LIB-store-frontend) $(LIB-latency) $(LIB-tcptransport) $(LIB-udptransport) $(LIB-smallbank)  $(o)../../sync_transaction_bench_client.o $(o)../../bench_client.o $(o)write_check_test.o $(GTEST_MAIN) $(GMOCK) $(GTEST)


TEST_BINS +=  $(addprefix $(d), smallbank_generator_test smallbank_client_test smallbank_utils_test amalgamate_test bal_test deposit_test transact_test write_check_test)