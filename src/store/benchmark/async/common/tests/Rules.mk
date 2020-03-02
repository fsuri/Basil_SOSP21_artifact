d := $(dir $(lastword $(MAKEFILE_LIST)))

GTEST_SRCS += $(addprefix $(d), key-selector-test.cc)

$(d)key-selector-test: $(o)key-selector-test.o $(LIB-key-selector) \
		$(GTEST_MAIN)

TEST_BINS += $(d)key-selector-test 
