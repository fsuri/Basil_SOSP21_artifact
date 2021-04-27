#!/bin/bash
cd $1/src/store/bftsmartstore/library
cp remote/java-config-us-east-1-0/java-config/hosts.config host0.config
cp remote/java-config-us-east-1-1/java-config/hosts.config host1.config
cp remote/java-config-us-east-1-2/java-config/hosts.config host2.config
cd $1/src/store/bftsmartstore/library/remote
for f in java-config-client-*; do echo $f; cd $f/java-config/; for i in {0..2}; do echo $i; mkdir java-config-group-$i; cp $1/src/store/bftsmartstore/library/host$i.config java-config-group-$i/hosts.config; cp system.config java-config-group-$i;done; cd $1/src/store/bftsmartstore/library/remote/; done

cd $1/src/scripts/
