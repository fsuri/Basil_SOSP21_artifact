#!/bin/bash

cat ./hosts | while read machine
do
    echo "#### send config to machine ${machine}"
    #scp  -r config Yunhao@${machine}.indicus.morty-pg0.utah.cloudlab.us:/users/Yunhao/
    rsync -rtuv config Yunhao@${machine}.augustus.morty-pg0.utah.cloudlab.us:/users/Yunhao/
done

