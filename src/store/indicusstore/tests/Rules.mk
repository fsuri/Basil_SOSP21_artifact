d := $(dir $(lastword $(MAKEFILE_LIST)))

GTEST_SRCS += $(addprefix $(d), common-test.cc server-test.cc common.cc)

$(d)common-test: $(o)common-test.o $(LIB-indicus-store) \
		$(GTEST_MAIN) $(o)common.o $(GMOCK)

$(d)server-test: $(o)server-test.o $(LIB-indicus-store) \
		$(GTEST_MAIN) $(o)common.o $(GMOCK)

TEST_BINS += $(d)common-test $(d)server-test 
