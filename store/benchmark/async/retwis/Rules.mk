d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), retwis_transaction.cc add_user.cc follow.cc \
													post_tweet.cc get_timeline.cc)

LIB-retwis := $(o)retwis_transaction.o $(o)add_user.o $(o)follow.o \
							$(o)post_tweet.o $(o)get_timeline.o
