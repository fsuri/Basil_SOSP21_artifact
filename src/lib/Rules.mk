d := $(dir $(lastword $(MAKEFILE_LIST)))

SRCS += $(addprefix $(d), \
	lookup3.cc message.cc memory.cc \
	latency.cc configuration.cc transport.cc \
	udptransport.cc tcptransport.cc simtransport.cc repltransport.cc \
	persistent_register.cc io_utils.cc crypto.cc keymanager.cc threadpool.cc \
	crypto_bench.cc threadpool_test.cc batched_sigs.cc batched_sigs_test.cc blake3_test.cc)

PROTOS += $(addprefix $(d), \
          latency-format.proto)

LIB-hash := $(o)lookup3.o

LIB-message := $(o)message.o $(LIB-hash)

LIB-hashtable := $(LIB-hash) $(LIB-message)

LIB-memory := $(o)memory.o

LIB-io-utils := $(o)io_utils.o

LIB-latency := $(o)latency.o $(o)latency-format.o $(LIB-message)

LIB-configuration := $(o)configuration.o $(LIB-message)

LIB-transport := $(o)transport.o $(o)threadpool.o $(LIB-message) $(LIB-configuration)

LIB-simtransport := $(o)simtransport.o $(LIB-transport)

LIB-repltransport := $(o)repltransport.o $(LIB-transport)

LIB-udptransport := $(o)udptransport.o $(LIB-transport)

LIB-tcptransport := $(o)tcptransport.o $(LIB-transport)

LIB-persistent_register := $(o)persistent_register.o $(LIB-message)

LIB-crypto := $(LIB-message) $(o)crypto.o $(o)keymanager.o $(d)ed25519.o

LIB-batched-sigs := $(LIB-crypto) $(o)batched_sigs.o 

$(d)crypto_bench: $(LIB-latency) $(LIB-crypto) $(LIB-batched-sigs) $(o)crypto_bench.o

$(d)threadpool_test: $(LIB-latency) $(LIB-crypto) $(LIB-batched-sigs) $(o)threadpool_test.o

#$(d)threadpool_test: $(LIB-transport) $(o)threadpool_test.o

$(d)batched_sigs_test: $(LIB-latency) $(LIB-crypto) $(LIB-batched-sigs) $(o)batched_sigs_test.o

$(d)blake3_test: $(LIB-latency) $(LIB-crypto) $(LIB-batched-sigs) $(o)blake3_test.o

#$(d)ed25519_donna: $(o)ed25519.o

BINS +=  $(d)crypto_bench $(d)threadpool_test $(d)batched_sigs_test $(d)blake3_test 

include $(d)tests/Rules.mk
