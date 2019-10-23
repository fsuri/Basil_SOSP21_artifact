d := $(dir $(lastword $(MAKEFILE_LIST)))

#
# gtest-based tests
#
GTEST_SRCS += $(addprefix $(d), \
					janus-server-test.cc \
					janus-client-test.cc)

# PROTOS += $(addprefix $(d), ../janus-proto.proto)

$(d)janus-server-test: $(o)janus-server-test.o \
	$(LIB-janus-store) $(LIB-simtransport) \
	$(LIB-store-common) \
	$(GTEST_MAIN)

# TEST_BINS += $(d)janus-server-test 

$(d)janus-client-test: $(o)janus-client-test.o $(LIB-janus-client) $(LIB-simtransport) $(LIB-store-common) $(GTEST_MAIN)

TEST_BINS += $(d)janus-client-test
