d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), key_selector.cc uniform_key_selector.cc \
													zipf_key_selector.cc)

LIB-key-selector := $(o)key_selector.o $(o)uniform_key_selector.o \
										$(o)zipf_key_selector.o

include $(d)tests/Rules.mk
