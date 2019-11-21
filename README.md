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
In addition, you need to install the following libraries from source:
- [googletest-1.10](https://github.com/google/googletest/releases/tag/release-1.10.0)
- [protobuf-3.5.1](https://github.com/protocolbuffers/protobuf/releases/tag/v3.5.1)

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
