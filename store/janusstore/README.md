# Overview
We implement the Janus protocol for fault-tolerant, replicated distributed transaction processing. We leverage the existing networking infrastructure provided by the TAPIR repository.

# Notes
10/2:
	- (TODO) verify transactions commit and are strictly serializable for multiclient and multishard system
	- (TODO) implement inquire
	- (TODO) unit testing server and client logic
9/25:
	- ok to use different transport per client
	- (TODO) verify transactions commit and are strictly serializable
	- (DONE) include key-val results in the commit OK message (and output commit callback)
	- (DONE) try two shards with 1 replica each without need for Inquire
	- (DONE) then try to change config to support cross-shard communication for Inquire (later)
- 9/18:
	- (DONE) got client-single replica on single shard working
	- (DONE) scale to multiple replicas on a shard
	- (DONE) end goal by next week: multiple clients running txns for 1 shard with multiple replicas
- 9/11:
	- reclass server under TransportReceiver as in weakstore
	- implement inquire function
	- implement fast quorum check and accept stage in preaccept callback for client.cc
	- finish benchmark_oneshot
		- currently, client can start up arbitrary transactions
		- goal by next week: client sends txns to server and server commits and client executes post-commit callback

- when we merge to master, may need to modify client API to fit evaluation framework

- note any interesting observations while doing this thing
	- potential typos/unexplained cases in the paper?
	- what overhead does Janus incur for more complex transactions outside of one-shot transactions? how does this affect performance?

# How to Run

The clients and servers have to be provided a configuration file, one
for each shard and a timestamp server (for OCC). For example a 3 shard
configuration will have the following files:

You will need to create a configuration file with the following
syntax:

```
f <number of failures tolerated>
group
replica <hostname>:<port>
replica <hostname>:<port>
...
group
replica <hostname>:<port>
replica <hostname>:<port>
...
multicast <multicast addr>:<port>
```

Each group is a replicated shard, and should contain `2f+1` replicas. Multicast
address is optional. However, the multi-sequenced groupcast implementation
uses the multicast address as the groupcast address.

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
