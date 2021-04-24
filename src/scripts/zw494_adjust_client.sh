#!/bin/bash
cd ../store/bftsmartstore/library
for f in java-config-client-*; do echo $f; cd $f/java-config/; for i in {0..2}; do echo $i; mkdir java-config-group-$i; cp ../../host$i.config java-config-group-$i/hosts.config; cp system.config java-config-group-$i;done; cd ../../; done

cd ../../../
