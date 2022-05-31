#!/bin/bash

F=0
NUM_GROUPS=1
CONFIG="shard-r1.config"
PROTOCOL="indicus"
STORE=${PROTOCOL}store
DURATION=10
ZIPF=0.0
NUM_OPS_TX=2
NUM_KEYS_IN_DB=1
KEY_PATH="keys"




while getopts f:g:cpath:p:d:z:num_ops:num_keys: option; do
case "${option}" in
f) F=${OPTARG};;
g) NUM_GROUPS=${OPTARG};;
cpath) CONFIG=${OPTARG};;
p) PROTOCOL=${OPTARG};;
d) DURATION=${OPTARG};;
z) ZIPF=${OPTARG};;
num_ops) NUM_OPS_TX=${OPTARG};;
num_keys) NUM_KEYS_IN_DB=${OPTARG};;
esac;
done

N=$((5*$F+1))

echo '[1] Shutting down possibly open servers'
for j in `seq 0 $((NUM_GROUPS-1))`; do
	for i in `seq 0 $((N-1))`; do
		#echo $((8000+$j*$N+$i))
		lsof -ti:$((8000+i)) | xargs kill -9 &>/dev/null   
	done;
done;
killall store/server

echo '[2] Starting new servers'
for j in `seq 0 $((NUM_GROUPS-1))`; do
	#echo Starting Group $j
	for i in `seq 0 $((N-1))`; do
		#echo Starting Replica $(($i+$j*$N))
		DEBUG=store/$STORE/* store/server --config_path $CONFIG --group_idx $j --num_groups $GROUPS --num_shards $GROUPS --replica_idx $i --protocol $PROTOCOL --num_keys $NUM_KEYS_IN_DB --debug_stats --indicus_key_path $KEY_PATH &> server$(($i+$j*$N)).out &
	done;
done;
