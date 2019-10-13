d := $(dir $(lastword $(MAKEFILE_LIST)))

GTEST_SRCS += $(addprefix $(d), smallbank_generator_test.cc)

$(d)smallbank_generator_test: $(o)smallbank_generator_test.o \
	$(LIB-io-utils) $(o)../smallbank_generator.o $(o)../smallbank-proto.o $(o)../utils.o \
	$(GTEST_MAIN)

TEST_BINS += $(d)smallbank_generator_test