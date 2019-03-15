# Overview
We implement the Janus protocol for fault-tolerant, replicated distributed transaction processing. We leverage the existing networking infrastructure provided by the TAPIR repository.

# Notes
- `shardclient` will forward txn to a shard replica, who will then act as the coordinator for the txn

# Timeline and TODOs
- 3/17: Define headers and system design for Janus modules
	- serialization graph representation
	- `shardclient` and `client` changes (if necessary) from TAPIR
	- `server` replica with coordinator capabilities
	- `store` changes (if necessary) from TAPIR
- 3/22: Implement communication between `client` and replica `server`
- 3/24: 

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
