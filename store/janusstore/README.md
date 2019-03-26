# Overview
We implement the Janus protocol for fault-tolerant, replicated distributed transaction processing. We leverage the existing networking infrastructure provided by the TAPIR repository.

# Notes
- client/coordinator for one person, server for the other

- when we merge to master, may need to modify client API to fit evaluation framework

- client should only worry about sending a full one-shot transaction to replicas

- store/server.cc contains info about actually starting up the janus server
- store/tapirstore/server.cc is just how we can match on the request op and call our handler logic
- need to define protobuf for our messages

- c++ will complain about not being able to convert Transaction to a key in a map; can just use tid

- if c++ complains a lot, ask matt

- as of 3/21:
	- design decision: individual shardclients will wrap the get/puts from client into a single Transaction object to be forwarded to the participating replicas
	- `client` handles transactions by ID but `shardclient` and `server` (replica) handle transactions by the full Transaction object in order to capture the appropriate operations
	- `store` is just a map object wrapped by a class; unsure if more is needed
	- `server` extends `AppReplica`, has an instance of the `store`, keeps the dependency graph as an adjacency list (implementation is a map from Transaction to list of Transactions), and has handlers for each phase of the protocol
	- `transaction` defines the Transaction object; mostly similar to how the tapirstore defines it
	- `client` and `shardclient` are similarly defined as their TAPIR counterparts without timestamps/truetime and TAPIR protocol functions

- in TAPIR `client` instantiates several `shardclients` to forward one-shot txn = {pieces} to the appropriate replica(s)
- coordinator dispatches the pieces to the appropriate replicas

- client and coordinator are co-located, as in the paper => 

- can have `shardclient` aggregate responses from replicas within the shard, so the client can aggregate those to determine conflicts/fast path

- will want to extend common/replica/AppReplica and can ignore LeaderUpCall() function bc no leader in janus and ReplicaUpCall() bc replicas can independently act on receipt of COMMIT message from coordinator/proxy

- params in the UpCall() functions are just for generic input/output

- note any interesting observations while doing this thing

# Timeline and TODOs
- 3/17: Define headers and system design for Janus modules
	- serialization graph representation
	- `shardclient` and `client` changes (if necessary) from TAPIR
	- `server` replica with coordinator capabilities
	- `store` changes (if necessary) from TAPIR
- 3/19: Install deps and get everything to compile
- Week of 3/25: Implement baseline communication between modules
- Week of 4/7: Implement working/runnable transaction system
- 4/12: System done
- Week of 4/14 - 4/21: Testing/benchmarking

# How to Run

The clients and servers have to be provided a configuration file, one
for each shard and a timestamp server (for OCC). For example a 3 shard
configuration will have the following files:

shard0.config
```
f 1  
replica <server-address-1>:<port>
replica <server-address-2>:<port>
replica <server-address-3>:<port>
```
shard1.config
```
f 1
replica <server-address-4>:<port>
replica <server-address-5>:<port>
replica <server-address-6>:<port>
```
shard2.config
```
f 1
replica <server-address-7>:<port>
replica <server-address-8>:<port>
replica <server-address-9>:<port>
```
shard.tss.config
```
f 1
replica <server-address-10>:<port>
replica <server-address-11>:<port>
replica <server-address-12>:<port>
```

## Running Servers
To start the replicas, run the following command with the `server`
binary for any of the stores,

`./server -c <shard-config-$n> -i <replica-number> -m <mode> -f <preload-keys>`

For each shard, you need to run `2f+1` instances of `server`
corresponding to the address:port pointed by `replica-number`.
Make sure you run all replicas for all shards.


## Running Clients
To run any of the clients in the benchmark directory,

`./client -c <shard-config-prefix> -N <n_shards> -m <mode>`
