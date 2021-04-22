#!/bin/bash

cat ./hosts | while read machine
do
    echo "#### send config to machine ${machine}"
    #scp  -r config fs435@${machine}.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/
    rsync -rtuv config zw494@${machine}.bftsmart.morty-pg0.utah.cloudlab.us:/users/zw494/
    rsync -rtuv ../store/bftsmartstore/library/config zw494@${machine}.bftsmart.morty-pg0.utah.cloudlab.us:/users/zw494/java-config/
    rsync -rtuv ../store/bftsmartstore/library/lib zw494@${machine}.bftsmart.morty-pg0.utah.cloudlab.us:/users/zw494/jars/
done

