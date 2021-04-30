#!/bin/bash

cat ./hosts | while read machine
do
    echo "#### send config to machine ${machine}"
    REMOTE_IP_ADDR=$(dig +short ${machine}.indicus.morty-pg0.utah.cloudlab.us | tail -n1)
    echo $REMOTE_IP_ADDR
    #scp  -r config fs435@${machine}.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/
    rsync -rtuv --delete config fs435@${machine}.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/
    rsync -rtuv --delete ../store/bftsmartstore/library/remote/java-config-${machine}/java-config fs435@${machine}.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/
    rsync -rtuv --delete ../store/bftsmartstore/library/jars fs435@${machine}.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/
done

