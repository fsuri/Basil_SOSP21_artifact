d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), retwis_transaction.cc add_user.cc follow.cc \
													post_tweet.cc get_timeline.cc)

OBJ-retwis-transaction := $(LIB-store-frontend) $(o)retwis_transaction.o

LIB-retwis := $(OBJ-retwis-transaction) $(o)add_user.o $(o)follow.o \
							$(o)post_tweet.o $(o)get_timeline.o
