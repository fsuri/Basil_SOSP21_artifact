# Overview
We implement the Janus protocol for fault-tolerant, replicated distributed transaction processing. We leverage the existing networking infrastructure provided by the TAPIR repository.

# Notes

- 3/29:
	- client partial impl
	- server partial impl
	- need config files to run
	- eric's questions:
		- 1) when would the txn already be in the graph
		- 2) preaccept phase: why should ballot be < highestballot(T)
		- 3) put txn status in the proto and Transaction class
		- 4) ok to have a map from key to txnIDs that are touching the key?

- when we merge to master, may need to modify client API to fit evaluation framework

- store/server.cc contains info about actually starting up the janus server
- store/tapirstore/server.cc is just how we can match on the request op and call our handler logic

- c++ will complain about not being able to convert Transaction to a key in a map; can just use tid

- if c++ complains a lot, ask matt

- params in the UpCall() functions are just for generic input/output

- note any interesting observations while doing this thing
	- potential typos/unexplained cases in the paper?

# Timeline and TODOs
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
