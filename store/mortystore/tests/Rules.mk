d := $(dir $(lastword $(MAKEFILE_LIST)))

GTEST_SRCS += $(addprefix $(d), branch-generator-test.cc branch-compatible-test.cc)
