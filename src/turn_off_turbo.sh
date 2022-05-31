#!/bin/bash

echo "Disabling Turbo Boost"
echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
