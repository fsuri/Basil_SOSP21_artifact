#!/bin/bash
for i in {4..7}; do
   echo "Disabling logical HT core $i."
   echo 1 > /sys/devices/system/cpu/cpu${i}/online;
done

