#!/bin/bash

cat ./hosts | while read machine
do
    echo "#### send config to machine ${machine}"
    scp  -r config fs435@${machine}.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/
done

