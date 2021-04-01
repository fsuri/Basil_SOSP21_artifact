#!/bin/bash

export DEBUG=store/bftsmartstore/*

store/server --config_path shard-r4.config --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 0 --protocol bftsmart --num_keys 1 --debug_stats --indicus_key_path scripts/local_keys&

export DEBUG=store/bftsmartstore/*

store/server --config_path shard-r4.config --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 1 --protocol bftsmart --num_keys 1 --debug_stats --indicus_key_path scripts/local_keys&

export DEBUG=store/bftsmartstore/*

store/server --config_path shard-r4.config --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 2 --protocol bftsmart --num_keys 1 --debug_stats --indicus_key_path scripts/local_keys&

export DEBUG=store/bftsmartstore/*

store/server --config_path shard-r4.config --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 3 --protocol bftsmart --num_keys 1 --debug_stats --indicus_key_path scripts/local_keys&

# node18-ntp-hotstuff
