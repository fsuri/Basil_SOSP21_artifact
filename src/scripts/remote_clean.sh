#!/bin/bash

cat ./hosts | while read machine
do
    echo "#### clean data on machine ${machine}"

    ssh Yunhao@${machine}.hotstuff.morty-pg0.utah.cloudlab.us "rm -rf /mnt/extra/experiments/*" &
done

