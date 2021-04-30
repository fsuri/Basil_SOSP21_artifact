#!/bin/bash
rm -r ~/BFT-SMART-STORE/BFT-DB/src/store/bftsmartstore/library/remote/java-config-*
rm -r ~/BFT-SMART-STORE/BFT-DB/src/store/bftsmartstore/library/remote
mkdir ~/BFT-SMART-STORE/BFT-DB/src/store/bftsmartstore/library/remote
cat ~/BFT-SMART-STORE/BFT-DB/src/scripts/hosts | while read machine
do
    echo "generating config file for machine ${machine}"
    mkdir ~/BFT-SMART-STORE/BFT-DB/src/store/bftsmartstore/library/remote/java-config-${machine}
    cp -r ~/BFT-SMART-STORE/BFT-DB/src/store/bftsmartstore/library/java-config ~/BFT-SMART-STORE/BFT-DB/src/store/bftsmartstore/library/remote/java-config-${machine}/java-config
done

