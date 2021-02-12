# TAPIR

This repository includes code implementing TAPIR -- the Transaction
Application Protocol for Inconsistent Replication. This code was used
for the SOSP 2015 paper, ["Building Consistent Transactions with
Inconsistent Replication."](http://dl.acm.org/authorize?N93281)

TAPIR is a new protocol for linearizable distributed transactions
built using replication with no consistency guarantees. By enforcing
consistency only at the transaction layer, TAPIR eliminates
coordination at the replication layer, enabling TAPIR to provide the
same transaction model and consistency guarantees as existing systems,
like Spanner, with better latency and throughput.

In addition to TAPIR, this repo includes several other useful
implementations of distributed systems, including:

1. An implementation of a lock server designed to work with
   inconsistent replication (IR), our high-performance, unordered
   replication protocol.

2. An implementation of Viewstamped Replication (VR), detailed in this
   [older paper](http://dl.acm.org/citation.cfm?id=62549) and this
   [more recent paper](http://18.7.29.232/handle/1721.1/71763).

3. An implementation of a scalable, distributed storage system
   designed to work with VR that uses two-phase commit to support
   distributed transactions and supports both optimistic concurrency
   control and strict two-phase locking.

The repo is structured as follows:

- /lib - the transport library for communication between nodes. This
  includes UDP based network communcation as well as the ability to
  simulate network conditions on a local machine, including packet
  delays and reorderings.

- /replication - replication library for the distributed stores
  - /vr - implementation of viewstamped replication protocol
  - /ir - implementation of inconsistent replication protocol

- /store - partitioned/sharded distributed store
  - /common - common data structures, backing stores and interfaces for all of stores
  - /tapirstore - implementation of TAPIR designed to work with IR
  - /strongstore - implementation of both an OCC-based and locking-based 2PC transactional
  storage system, designed to work with VR
  - /weakstore - implementation of an eventually consistent storage
    system, using quorum writes for replication

- /lockserver - a lock server designed to be used with IR

## Compiling & Running
You can compile all of the TAPIR executables by running make in the root directory

TAPIR depends on protobufs, libevent and openssl, so you will need the following development libraries:
- libevent-openssl
- libevent-pthreads
- libevent-dev
- libssl-dev
- libgflags-dev
- libsodium-dev

In addition, you need to install the following libraries from source:
- [googletest-1.10](https://github.com/google/googletest/releases/tag/release-1.10.0)
- [protobuf-3.5.1](https://github.com/protocolbuffers/protobuf/releases/tag/v3.5.1)
- [cryptopp-8.2](htps://cryptopp.com/cryptopp820.zip)
- [bitcoin-core/secp256k1](https://github.com/bitcoin-core/secp256k1/)
- [BLAKE3](https://github.com/BLAKE3-team/BLAKE3)
- [ed25519-donna] (https://github.com/floodyberry/ed25519-donna)
- [Intel TBB] (https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit/get-the-toolkit.html). In order to compile, will need to configure CPU: https://software.intel.com/content/www/us/en/develop/documentation/get-started-with-intel-oneapi-base-linux/top/before-you-begin.html

Indicus additionally relies on:
- [moodycamel/concurrentqueue] (https://github.com/cameron314/concurrentqueue). Already part of code-base, nothing to be done.

To install BLAKE3
1. `git clone https://github.com/BLAKE3-team/BLAKE3`
2. `cd BLAKE3/c`
3. `gcc -fPIC -shared -O3 -o libblake3.so blake3.c blake3_dispatch.c blake3_portable.c blake3_sse2_x86-64_unix.S blake3_sse41_x86-64_unix.S blake3_avx2_x86-64_unix.S blake3_avx512_x86-64_unix.S`
4. `sudo cp libblake3.so /usr/local/lib/`
5. `sudo ldconfig`

To install ed25519-donna 
1. `git clone https://github.com/floodyberry/ed25519-donna`
2. `cd ed25519-donna`
3. `gcc -fPIC -shared -O3 -m64 -o libed25519_donna.so ed25519.c -lssl -lcrypto`
4. `sudo cp libed25519_donna.so /usr/local/lib`
5. `sudo ldconfig`

### HotStuff

#### Before compile

dependencies: `sudo apt-get install libssl-dev libuv1-dev cmake make`; The current string in `src/store/hotstuffstore/libhotstuff/examples/indicus_interface.h` is `/users/Yunhao/config/`. This is the directory I am using for my CloudLab config and one may change `Yunhao` to their own username.

#### Compile

First, goto directory `src/store/hotstuffstore/libhotstuff` and run `./build.sh`. Then run `make` in `/src` as usual.

#### Crypto configuration for HotStuff

I updated the experiment scripts for HotStuff and now the script `run_multiple_experiments.py` will create 3 shards and each 6 replicas -- 18 machines in total. The HotStuff config for this is in `src/scripts/config`. Use `src/scripts/config_remote.sh` to upload these config to remote machines before your experiments. If needed, remember to update `src/scripts/hosts` for the remote hosts and replace the *two* `Yunhao` in `src/scripts/config_remote.sh` to your username.

If you need the crypto configuration in another setup (different host names / different number of replias, etc.), let me know and I will write the instructions.

#### Local test

Here are the steps for running a local experiment.

1. In `src/store/hotstuff/libhotstuff/examples/indicus_interface.h`, modify the string `config_dir_base` to your local directory of `???/src/store/hotstuffstore/libhotstuff/conf-indicus/`.

2. Compile the code use `build.sh` and `make` as mentioned above.

3. Open one shell and goto `src/`, type `./scripts/hotstuff_server.sh`

4. Open another shell and goto `src/`, type `./scripts/hotstuff_client.sh`

5. The server shell should have debug output of HotStuff and the client shell should print continuously something like `rw,185119562,1112640505801878,16`.

6. Run `./scripts/kill.sh` to terminate and clean the local test.


### On Mac
The known Mac equivalents for the above packages, available through `brew install` are:
- libevent
- protobuf
- gflags
- openssl
```# For compilers to find openssl you may need to set:
export LDFLAGS="-L/usr/local/opt/openssl/lib"
export CPPFLAGS="-I/usr/local/opt/openssl/include"

# For pkg-config to find openssl you may need to set:
export PKG_CONFIG_PATH="/usr/local/opt/openssl/lib/pkgconfig"
```

You'll also want to setup `googletest`. The project is known to compile with [version 1.7.0](https://github.com/google/googletest/releases/tag/release-1.7.0) with these steps. You will need to redo these steps whenever you `make clean`:
1. Download `googletest` 1.7.0
2. Unzip and move the folder into `.obj` as `gtest`
3. `cd gtest`
4. `mkdir bld`
5. `cd bld`
6. `cmake ..`
7. `make`
8. Navigate to the `gtest` directory
9. `g++ -isystem ./include -I . -pthread -c ./src/gtest-all.cc`
10. `g++ -isystem ./include -I . -pthread -c ./src/gtest_main.cc`

## Contact and Questions
Please email Irene at iyzhang@cs.washington.edu, Dan at drkp@cs.washington.edu and Naveen at naveenks@cs.washington.edu
