#!/bin/bash
rm -r $1/src/store/bftsmartstore/library/java-config-*
rm -r $1/src/store/bftsmartstore/library/remote
mkdir $1/src/store/bftsmartstore/library/remote
cat $1/src/scripts/server-hosts | while read machine
do
    if [[ $machine =~ [:digit:] ]]; then 
    echo "generating config file for machine ${machine}";
    mkdir $1/src/store/bftsmartstore/library/remote/java-config-${machine};
    cp -r $1/src/store/bftsmartstore/library/java-config $1/src/store/bftsmartstore/library/remote/java-config-${machine}/java-config;
    fi
done
cat $1/src/scripts/client-hosts | while read machine
do
    echo "generating config file for machine ${machine}"
    mkdir $1/src/store/bftsmartstore/library/remote/java-config-${machine}
    cp -r $1/src/store/bftsmartstore/library/java-config $1/src/store/bftsmartstore/library/remote/java-config-${machine}/java-config
done
