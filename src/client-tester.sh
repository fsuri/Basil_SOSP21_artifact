#!/bin/bash

CLIENTS=1
F=0
NUM_GROUPS=1
CONFIG="shard-r1.config"
PROTOCOL="indicus"
STORE=PROTOCOL+"store"
DURATION=10
ZIPF=0.0
NUM_OPS_TX=2
NUM_KEYS_IN_DB=1
KEY_PATH="keys"




while getopts c:f:g:cpath:p:d:z:num_ops:num_keys: option; do
case "${option}" in
c) CLIENTS=${OPTARG};;
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

echo '[1] Starting new clients'
for i in `seq 1 $((CLIENTS-1))`; do
  #valgrind
  #DEBUG=store/mortystore/*
  store/benchmark/async/benchmark --config_path $CONFIG --num_groups $NUM_GROUPS \
    --num_shards $NUM_GROUPS \
    --protocol_mode $PROTOCOL --num_keys $NUM_KEYS_IN_DB --benchmark rw --num_ops_txn $NUM_OPS_TX \
    --exp_duration $DURATION --client_id $i --warmup_secs 0 --cooldown_secs 0 \
    --key_selector zipf --zipf_coefficient $ZIPF --indicus_key_path $KEY_PATH &> client-$i.out &
done;
#valgrind
DEBUG=store/$STORE/* store/benchmark/async/benchmark --config_path $CONFIG --num_groups $NUM_GROUPS \
  --num_shards $NUM_GROUPS --protocol_mode $PROTOCOL --num_keys $NUM_KEYS_IN_DB --benchmark rw \
  --num_ops_txn $NUM_OPS_TX --exp_duration $DURATION --client_id 0 --warmup_secs 0 \
  --cooldown_secs 0 --key_selector zipf --zipf_coefficient $ZIPF \
  --stats_file "stats-0.json" --indicus_key_path $KEY_PATH &> client-0.out &


sleep $((DURATION+3))
echo '[2] Shutting down possibly open servers and clients'
killall store/benchmark/async/benchmark
killall store/server

