#!/bin/bash
$1/src/scripts/zw494_clean.sh $1
python3 $1/src/scripts/zw494_gen_bft_conf_files.py $1/src/scripts $3 $4 $5
$1/src/scripts/zw494_create_configs.sh $1
python3 $1/src/scripts/zw494_gen_system_config.py $1 $1/src/scripts $3 $4 $5
$1/src/scripts/zw494_config_remote.sh $1/src/scripts $2 $3 $4 $5
