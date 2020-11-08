store/benchmark/async/benchmark --client_id 0 --config_path shard-r4.config --num_groups 1 --num_shards 1 --protocol_mode hotstuff --num_keys 1 --benchmark rw --num_ops_txn 2 --exp_duration 10 --warmup_secs 0 --cooldown_secs 0 --key_selector zipf --zipf_coefficient 0.0 --stats_file "stats-0.json" --indicus_key_path keys &


store/benchmark/async/benchmark --client_id 1 --config_path shard-r4.config --num_groups 1 --num_shards 1 --protocol_mode hotstuff --num_keys 1 --benchmark rw --num_ops_txn 2 --exp_duration 10 --warmup_secs 0 --cooldown_secs 0 --key_selector zipf --zipf_coefficient 0.0 --stats_file "stats-0.json" --indicus_key_path keys &


store/benchmark/async/benchmark --client_id 2 --config_path shard-r4.config --num_groups 1 --num_shards 1 --protocol_mode hotstuff --num_keys 1 --benchmark rw --num_ops_txn 2 --exp_duration 10 --warmup_secs 0 --cooldown_secs 0 --key_selector zipf --zipf_coefficient 0.0 --stats_file "stats-0.json" --indicus_key_path keys &


store/benchmark/async/benchmark --client_id 3 --config_path shard-r4.config --num_groups 1 --num_shards 1 --protocol_mode hotstuff --num_keys 1 --benchmark rw --num_ops_txn 2 --exp_duration 10 --warmup_secs 0 --cooldown_secs 0 --key_selector zipf --zipf_coefficient 0.0 --stats_file "stats-0.json" --indicus_key_path keys &


store/benchmark/async/benchmark --client_id 4 --config_path shard-r4.config --num_groups 1 --num_shards 1 --protocol_mode hotstuff --num_keys 1 --benchmark rw --num_ops_txn 2 --exp_duration 10 --warmup_secs 0 --cooldown_secs 0 --key_selector zipf --zipf_coefficient 0.0 --stats_file "stats-0.json" --indicus_key_path keys &
