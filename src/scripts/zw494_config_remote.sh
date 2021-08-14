#!/bin/bash

cd $1

cat ./server-hosts | while read machine
do
    if [[ $machine =~ [:digit:] ]]; then 
        echo "#### send config to machine ${machine}"

        rsync -e "ssh -o StrictHostKeyChecking=no" -rtuv --delete config $2@${machine}.$3.$4.$5:/users/$2/
        rsync -e "ssh -o StrictHostKeyChecking=no" -rtuv --delete ../store/bftsmartstore/library/remote/java-config-${machine}/java-config $2@${machine}.$3.$4.$5:/users/$2/
        rsync -e "ssh -o StrictHostKeyChecking=no" -rtuv --delete ../store/bftsmartstore/library/jars $2@${machine}.$3.$4.$5:/users/$2/
    fi
done

cat ./client-hosts | while read machine
do
    echo "#### send config to machine ${machine}"

    rsync -e "ssh -o StrictHostKeyChecking=no" -rtuv --delete config $2@${machine}.$3.$4.$5:/users/$2/
    rsync -e "ssh -o StrictHostKeyChecking=no" -rtuv --delete ../store/bftsmartstore/library/remote/java-config-${machine}/java-config $2@${machine}.$3.$4.$5:/users/$2/
    rsync -e "ssh -o StrictHostKeyChecking=no" -rtuv --delete ../store/bftsmartstore/library/jars $2@${machine}.$3.$4.$5:/users/$2/
done
