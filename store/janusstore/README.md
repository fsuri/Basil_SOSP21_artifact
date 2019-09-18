# Overview
We implement the Janus protocol for fault-tolerant, replicated distributed transaction processing. We leverage the existing networking infrastructure provided by the TAPIR repository.

# Notes
- 9/18:
	- got client-single replica on single shard working
	- verify that transactions commit and are strictly serializable for one replica
	- scale to multiple replicas on a shard
	- then try to change config to support cross-shard communication for Inquire

- 9/11:
	- reclass server under TransportReceiver as in weakstore
	- implement inquire function
	- implement fast quorum check and accept stage in preaccept callback for client.cc
	- finish benchmark_oneshot
		- currently, client can start up arbitrary transactions
		- goal by next week: client sends txns to server and server commits and client executes post-commit callback
- 6/10:
	- client impl done except for some TODOs that might need clarification
	- what's the progress/update on the shim layer?
	- should try to run a transaction thru the client next
- 5/10:
	- client impl mostly done, need to get to compile and write the shim layer
		- will probably need to sort out some bugs with client/shardclient state
		- for now, dont need to write shim layer; can just have a main function that randomly generates a one-shot txn and tells the client to send it
	- think what about janus makes it impossible to support general txns, aka interactive transactions
		- look at [2] and [41] in janus paper for discussions on transaction classes
- 4/10:
	- client impl
		- how do we return transaction results from the client?
			- use the callback function given from wrapper of client in PreAccept
		- type issues with callback functions; likely same for shardclient
	- shardclient impl
		- how does the shardclient receive responses from replicas?
			- answer: continuation callbacks that do the wrapping
		- is there a way to tell how many replicas there are in the shard?
			- otherwise, how can we decide when to forward responses to the client?
			- answer: this->config.n is the number of replicas;
			- additionally, this means we need to loop and call InvokeUnlogged for replica IDs 0...n-1
- 3/29:
	- client partial impl
	- server partial impl
	- need config files to run

- when we merge to master, may need to modify client API to fit evaluation framework

- store/server.cc contains info about actually starting up the janus server
- store/tapirstore/server.cc is just how we can match on the request op and call our handler logic

- note any interesting observations while doing this thing
	- potential typos/unexplained cases in the paper?
	- what overhead does Janus incur for more complex transactions outside of one-shot transactions? how does this affect performance?

<!-- questions: 
	can we do this/other stuff for meng project? i know we're supposed to find an advisor for meng; would that be alvisi?
-->

# Timeline and TODOs
- 5/17 - get client to compile and proofread logic
	- check notes from 4/10 because there are some client/shardclient implementation details we need to address
- 5/28 - run a transaction via client to server (doesnt have to actually work)

- 9/02 - janus impl done and runnable (and debuggable) with relative performance to tapir and morty (reproduce Zipf/throughput graphs in janus paper)
- 9/19 - paper deadline (3 wks after classes start)

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
