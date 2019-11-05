d := $(dir $(lastword $(MAKEFILE_LIST)))

GTEST_SRCS += $(addprefix $(d), branch-generator-test.cc branch-compatible-test.cc)

$(d)branch-generator-test: $(o)branch-generator-test.o $(LIB-morty-store) \
	$(LIB-configuration) $(GTEST_MAIN)

$(d)branch-compatible-test: $(o)branch-compatible-test.o $(LIB-morty-store) \
	$(LIB-configuration) $(GTEST_MAIN)

TEST_BINS += $(d)branch-generator-test $(d)branch-compatible-test

