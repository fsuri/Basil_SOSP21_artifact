#!/bin/bash

mkdir keys

for i in `seq 0 1024`; do 
create_keys/create_key 4 keys/$i; 
done
