d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), pingserver.cc \
				kvstore.cc lockserver.cc txnstore.cc versionstore.cc)

LIB-store-backend := $(o)kvstore.o $(o)lockserver.o $(o)txnstore.o $(o)versionstore.o \
	$(o)pingserver.o

include $(d)tests/Rules.mk
