#!/bin/bash

TARGET_DIR="/users/fs435/config/"

echo 'Update hotstuff header file remote_config_dir.h as'
echo '#define REMOTE_CONFIG_DIR "'$TARGET_DIR'"'

echo '#define REMOTE_CONFIG_DIR "'$TARGET_DIR'"' > ../store/hotstuffstore/libhotstuff/examples/remote_config_dir.h



cat ./hosts | while read machine
do
    echo "#### send config to machine ${machine}"
    #scp  -r config fs435@${machine}.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/
    rsync -rtuv config fs435@${machine}.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/
done

