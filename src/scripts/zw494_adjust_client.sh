#!/bin/bash
cd /home/zw494/BFT-DB/src/store/bftsmartstore/library/remote
for f in java-config-client-*; do echo $f; cd $f/java-config/; for i in {0..2}; do echo $i; mkdir java-config-group-$i; cp /home/zw494/BFT-DB/src/store/bftsmartstore/library/host$i.config java-config-group-$i/hosts.config; cp system.config java-config-group-$i;done; cd /home/zw494/BFT-DB/src/store/bftsmartstore/library/remote/; done

cd /home/zw494/BFT-DB/src/scripts/
