d := $(dir $(lastword $(MAKEFILE_LIST)))

#
# gtest-based tests
#
GTEST_SRCS += $(d)janus-server-test.cc

# PROTOS += $(addprefix $(d), ../janus-proto.proto)

$(d)janus-server-test: $(o)janus-server-test.o \
	$(LIB-janus-store) $(LIB-simtransport) \
	$(LIB-store-common) \
	$(GTEST_MAIN)

#TEST_BINS += $(d)janus-server-test

