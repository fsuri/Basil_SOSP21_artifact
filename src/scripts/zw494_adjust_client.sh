#!/bin/bash
cd /home/floriansuri/BFT-SMART-STORE/BFT-DB/src/store/bftsmartstore/library/remote
for f in java-config-client-*; do echo $f; cd $f/java-config/; for i in {0..2}; do echo $i; mkdir java-config-group-$i; cp /home/floriansuri/BFT-SMART-STORE/BFT-DB/src/store/bftsmartstore/library/host$i.config java-config-group-$i/hosts.config; cp system.config java-config-group-$i;done; cd /home/floriansuri/BFT-SMART-STORE/BFT-DB/src/store/bftsmartstore/library/remote/; done

cd /home/floriansuri/BFT-SMART-STORE/BFT-DB/src/scripts/
