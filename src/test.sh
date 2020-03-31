CLIENTS=0
DURATION=10
ZIPF=0.0
KEYS=2
NUM_KEYS=1

while getopts d:c:z:k:n: option; do
case "${option}" in
d) DURATION=${OPTARG};;
c) CLIENTS=${OPTARG};;
z) ZIPF=${OPTARG};;
k) KEYS=${OPTARG};;
n) NUM_KEYS=${OPTARG};;
esac;
done

lsof -ti:8000 | xargs kill -9
lsof -ti:8001 | xargs kill -9
lsof -ti:8002 | xargs kill -9
lsof -ti:8003 | xargs kill -9
lsof -ti:8004 | xargs kill -9
lsof -ti:8005 | xargs kill -9
killall store/server
killall store/benchmark/async/benchmark

#DEBUG=lib/tcptransport.cc
#valgrind --tool=callgrind --collect-systime=yes
DEBUG=store/indicusstore/* store/server --config_path shard-r0.config \
  --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 0 --protocol indicus \
  --num_keys $NUM_KEYS --debug_stats --indicus_key_path keys &> server.out &
DEBUG=store/indicusstore/* store/server --config_path shard-r0.config \
  --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 1 --protocol indicus \
  --num_keys $NUM_KEYS --debug_stats --indicus_key_path keys &> server.out &
DEBUG=store/indicusstore/* store/server --config_path shard-r0.config \
  --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 2 --protocol indicus \
  --num_keys $NUM_KEYS --debug_stats --indicus_key_path keys &> server.out &
DEBUG=store/indicusstore/* store/server --config_path shard-r0.config \
  --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 3 --protocol indicus \
  --num_keys $NUM_KEYS --debug_stats --indicus_key_path keys &> server.out &
DEBUG=store/indicusstore/* store/server --config_path shard-r0.config \
  --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 4 --protocol indicus \
  --num_keys $NUM_KEYS --debug_stats --indicus_key_path keys &> server.out &
DEBUG=store/indicusstore/* store/server --config_path shard-r0.config \
  --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 5 --protocol indicus \
  --num_keys $NUM_KEYS --debug_stats --indicus_key_path keys &> server.out &

sleep 1
for i in `seq 1 $CLIENTS`; do
  #valgrind
  #DEBUG=store/mortystore/*
  store/benchmark/async/benchmark --config_path shard-r0.config --num_groups 1 \
    --num_shards 1 \
    --protocol_mode indicus --num_keys $NUM_KEYS --benchmark rw --num_ops_txn $KEYS \
    --exp_duration $DURATION --client_id $i --warmup_secs 0 --cooldown_secs 0 \
    --key_selector zipf --zipf_coefficient $ZIPF --indicus_key_path keys &> client-$i.out &
done;
#valgrind
DEBUG=store/indicusstore/* store/benchmark/async/benchmark --config_path shard-r0.config --num_groups 1 \
  --num_shards 1 --protocol_mode indicus --num_keys $NUM_KEYS --benchmark rw \
  --num_ops_txn $KEYS --exp_duration $DURATION --client_id 0 --warmup_secs 0 \
  --cooldown_secs 0 --key_selector zipf --zipf_coefficient $ZIPF \
  --stats_file "stats-0.json" --indicus_key_path keys &> client-0.out

#callgrind_control --dump
#callgrind_control --kill
# killall store/server
